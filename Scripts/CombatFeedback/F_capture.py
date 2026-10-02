"""Record an observed game window + default WASAPI output in one FFmpeg process.

Usage with bundled Python 3.12: python Scripts/CombatFeedback/F_capture.py
Reads Saved/CombatFeedback/F/capture-request.json (window_title, seconds, label).
Deps are local to Saved/CombatFeedback/F/tools; this records, never injects input.
Both inputs use wall-clock timestamps. Saves media, command, audio-device and
signal analysis. OS loopback includes all output; keep other audio apps quiet.
Does not imply a human listened or played. Video/audio sync still needs review.
"""
from pathlib import Path
import datetime
import json
import queue
import subprocess
import sys
import threading
import time
import traceback

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Saved/CombatFeedback/F/tools'))
import imageio_ffmpeg
import pyaudiowpatch as pa
import numpy as np
request=json.loads((ROOT/'Saved/CombatFeedback/F/capture-request.json').read_text(encoding='utf-8'))
OUT=ROOT/'Saved/CombatFeedback/F/media'/(datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+request['label'])
OUT.mkdir(parents=True)
ffmpeg=imageio_ffmpeg.get_ffmpeg_exe()
audio=pa.PyAudio();device=audio.get_default_wasapi_loopback()
channels=min(2,int(device['maxInputChannels']));rate=int(device['defaultSampleRate'])
command=[ffmpeg,'-y','-hide_banner','-loglevel','info',
         '-thread_queue_size','1024','-use_wallclock_as_timestamps','1','-f','s16le','-ar',str(rate),'-ac',str(channels),'-probesize','32','-analyzeduration','0','-i','pipe:0',
         '-thread_queue_size','1024','-use_wallclock_as_timestamps','1','-f','gdigrab','-framerate','30','-draw_mouse','0','-i','title='+request['window_title'],
         '-copyts','-start_at_zero','-map','1:v:0','-map','0:a:0','-t',str(request['seconds']),
         '-c:v','libx264','-preset','ultrafast','-crf','22','-pix_fmt','yuv420p','-vf','pad=ceil(iw/2)*2:ceil(ih/2)*2',
         '-c:a','aac','-b:a','192k','-af','aresample=async=1:first_pts=0','-movflags','+faststart',str(OUT/'original-output.mp4')]
if request.get('method')=='desktop':
    box=request['physical_box']
    pos=command.index('-draw_mouse')
    command[pos:pos]=['-offset_x',str(box['x']),'-offset_y',str(box['y']),'-video_size',str(box['width'])+'x'+str(box['height'])]
    command[command.index('title='+request['window_title'])]='desktop'
if request.get('method')=='dda':
    # Screenshot-derived bounded region; never record the rest of the desktop.
    box=request['physical_box']
    capture='ddagrab=output_idx=0:draw_mouse=0:framerate=30:offset_x='+str(box['x'])+':offset_y='+str(box['y'])+':video_size='+str(box['width'])+'x'+str(box['height'])
    position=command.index('gdigrab')
    command[position]='lavfi';command[position+1:position+7]=['-i',capture]
    vf=command.index('-vf')
    command[vf+1]='hwdownload,format=bgra,pad=ceil(iw/2)*2:ceil(ih/2)*2'
chunks=queue.Queue();stop=threading.Event();pcm=[];times=[];errors=[]
def callback(data,count,info,status):
    if not times:times.append({'callback_wall':time.time(),'buffer_frames':count,'portaudio_times':info})
    pcm.append(data);chunks.put(data)
    return (None,pa.paComplete if stop.is_set() else pa.paContinue)
started=time.time()
with (OUT/'ffmpeg.log').open('w',encoding='utf-8') as log:
    process=subprocess.Popen(command,cwd=ROOT,stdin=subprocess.PIPE,stdout=subprocess.DEVNULL,stderr=log,
                             creationflags=subprocess.CREATE_NO_WINDOW)
    def writer():
        try:
            while process.poll() is None:
                try:data=chunks.get(timeout=.2)
                except queue.Empty:continue
                process.stdin.write(data);process.stdin.flush()
        except (BrokenPipeError,OSError) as error:errors.append(str(error))
    thread=threading.Thread(target=writer,daemon=True);thread.start()
    stream=audio.open(format=pa.paInt16,channels=channels,rate=rate,input=True,input_device_index=device['index'],
                      frames_per_buffer=1024,stream_callback=callback)
    (ROOT/'Saved/CombatFeedback/F/recorder-ready.json').write_text(json.dumps({'start_wall':started,'path':str(OUT),'compare_run':request.get('compare_run'),'label':request['label']},ensure_ascii=False),encoding='utf-8')
    try:code=process.wait(timeout=request['seconds']+30)
    except subprocess.TimeoutExpired:
        process.terminate();code=process.wait();errors.append('capture timeout')
    finally:
        stop.set();stream.stop_stream();stream.close();audio.terminate()
        if process.stdin:
            try:process.stdin.close()
            except OSError as error:errors.append('pipe cleanup after FFmpeg exit: '+str(error))
values=np.frombuffer(b''.join(pcm),dtype=np.int16).astype(np.float64)
analysis={'pcm_seconds':len(values)/(rate*channels),'nonzero_samples':int(np.count_nonzero(values)),
          'peak_dbfs':float(20*np.log10(max(1,np.max(np.abs(values)))/32768)) if len(values) else None,
          'clipped_samples':int(np.count_nonzero(np.abs(values)>=32767)),
          'rms_dbfs':float(20*np.log10(max(.00001,np.sqrt(np.mean(values**2)))/32768)) if len(values) else None}
metadata={'candidate':'FA2-20260930-v2','request':request,'command':command,'audio_device':device,
          'start_wall':started,'end_wall':time.time(),'first_callback':times,'exit_code':code,'errors':errors,
          'audio_signal':analysis,'method':request.get('method','gdigrab')+' observed game region + OS WASAPI loopback, common FFmpeg wallclock timestamps',
          'human_input':False,'human_listening':False,'sync':'common timestamp capture; subjective A/V sync check pending'}
media_info=subprocess.run([ffmpeg,'-hide_banner','-i',str(OUT/'original-output.mp4'),'-map','0:v:0','-f','null','-'],stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,text=True)
(OUT/'video-decode.log').write_text(media_info.stderr,encoding='utf-8')
import re
decoded=re.findall(r'frame=\s*(\d+)',media_info.stderr)
metadata['decoded_video_frames']=int(decoded[-1]) if decoded else 0
metadata['video_valid']=media_info.returncode==0 and metadata['decoded_video_frames']>=request['seconds']*20
(OUT/'recording.json').write_text(json.dumps(metadata,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'path':str(OUT),'exit_code':code,'audio_signal':analysis,'errors':errors},ensure_ascii=False,indent=2))
assert code==0,'Video capture failed; preserve log'
assert metadata['video_valid'],'Insufficient decoded moving-video frames; do not claim synchronized video'
assert analysis['nonzero_samples']>0,'Actual OS output was silent; do not claim audible output'
