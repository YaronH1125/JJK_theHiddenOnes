const fs=require('fs'),path=require('path'),{spawn}=require('child_process'),{pathToFileURL}=require('url');
const root=path.resolve(__dirname,'..'),out=path.join(root,'Saved/MenuBridgeCheck');
fs.mkdirSync(out,{recursive:true});
const pause=ms=>new Promise(r=>setTimeout(r,ms));
const proc=spawn('C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',[
 '--headless=new','--remote-debugging-port=9443','--user-data-dir='+path.join(out,'profile'),'about:blank'
],{windowsHide:true,stdio:'ignore'});
(async()=>{
 let tabs;for(let i=0;i<40;i++){try{tabs=await(await fetch('http://127.0.0.1:9443/json')).json();break;}catch{await pause(250);}}
 if(!tabs)throw Error('Edge did not start');
 const ws=new WebSocket(tabs.find(t=>t.type==='page').webSocketDebuggerUrl);
 await new Promise(r=>ws.onopen=r);let seq=0;const pending=new Map();
 ws.onmessage=e=>{const m=JSON.parse(e.data);if(m.id){const p=pending.get(m.id);if(p){pending.delete(m.id);m.error?p.reject(m.error):p.resolve(m.result);}}};
 const call=(method,params={})=>new Promise((resolve,reject)=>{const id=++seq;pending.set(id,{resolve,reject});ws.send(JSON.stringify({id,method,params}));});
 const ev=async expression=>{const r=await call('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});if(r.exceptionDetails)throw Error(JSON.stringify(r.exceptionDetails));return r.result.value;};
 const checks=[];const check=(name,pass)=>{checks.push({name,pass});if(!pass)throw Error(name);};
 try{
  await call('Runtime.enable');await call('Page.enable');
  await call('Emulation.setDeviceMetricsOverride',{width:1920,height:1080,deviceScaleFactor:1,mobile:false});
  await call('Page.navigate',{url:pathToFileURL(path.join(root,'Content/UI/Menu/menu-ui.html')).href});await pause(600);
  await ev(`window.ue={menu:{command:()=>Promise.resolve('{}')}};window.menuUE.boot({opponent:'AI 对战'});document.querySelector('[data-action="training-start"]').click();document.querySelector('[data-action="enter-battle"]').click();window.menuUE.open('pause','graphics','AI 对战');`);
  check('Training with AI keeps free-training session',await ev(`menuPrototype.getState().mode==='training'&&document.querySelector('.breadcrumb').textContent.includes('自由训练')`));
  await ev(`go('main');document.querySelector('[data-action="start"]').click();document.querySelector('[data-action="enter-battle"]').click();window.menuUE.open('pause','graphics','AI 对战');`);
  check('AI battle keeps battle session',await ev(`menuPrototype.getState().mode==='ai'&&document.querySelector('.breadcrumb').textContent.includes('AI 对战')`));
  check('All bundled images load',await ev(`[...document.images].every(i=>i.complete&&i.naturalWidth>0)`));
  fs.writeFileSync(path.join(out,'report.json'),JSON.stringify({browser:'Microsoft Edge',checks},null,2));
  console.log(JSON.stringify({passed:checks.length,total:checks.length}));
 }finally{await call('Browser.close').catch(()=>{});ws.close();}
})().catch(e=>{console.error(e);proc.kill();process.exitCode=1;});
