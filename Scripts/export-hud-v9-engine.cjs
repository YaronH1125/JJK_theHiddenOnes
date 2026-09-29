// Render existing vector UI and portrait clipping to transparent engine textures in Edge.
const fs=require('fs'),path=require('path');
(async()=>{
 const tabs=await(await fetch('http://127.0.0.1:9338/json')).json();
 const ws=new WebSocket(tabs.find(t=>t.type==='page').webSocketDebuggerUrl); await new Promise(r=>ws.onopen=r);
 let n=0;const pending=new Map();ws.onmessage=e=>{const d=JSON.parse(e.data);if(d.id){const p=pending.get(d.id);pending.delete(d.id);d.error?p.reject(d.error):p.resolve(d.result);}};
 const call=(method,params={})=>new Promise((resolve,reject)=>{const id=++n;pending.set(id,{resolve,reject});ws.send(JSON.stringify({id,method,params}));});
 const ev=async expression=>{const r=await call('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});if(r.exceptionDetails)throw Error(JSON.stringify(r.exceptionDetails));return r.result.value;};
 const dir=path.resolve(__dirname,'../Docs/assets/hud-v9');
 const html=fs.readFileSync(path.resolve(dir,'../hud-prototype-v9.0.html'),'utf8');
 const svg=html.match(/<svg class="stormFrame"[\s\S]*?<\/svg>/)[0].replace('<svg ','<svg xmlns="http://www.w3.org/2000/svg" ');
 const source=fs.readFileSync(path.resolve(dir,'../hud-v8/ishigori-portrait.png')).toString('base64');
 for(const [name,src,w,h,portrait] of [['scroll','data:image/svg+xml;base64,'+Buffer.from(svg).toString('base64'),1560,310,false],['portrait','data:image/png;base64,'+source,368,400,true]]){
  const png=await ev(`(async()=>{const im=new Image();im.src=${JSON.stringify(src)};await im.decode();const c=document.createElement('canvas');c.width=${w};c.height=${h};const x=c.getContext('2d');if(${portrait}){x.fillStyle='#c7b28c';x.beginPath();x.ellipse(184,200,184,200,0,0,Math.PI*2);x.fill();x.beginPath();x.ellipse(184,200,176,192,0,0,Math.PI*2);x.clip();const scale=400/im.height;x.drawImage(im,(368-im.width*scale)/2,0,im.width*scale,400);}else{x.drawImage(im,0,0,c.width,c.height);}return c.toDataURL('image/png').split(',')[1];})()`);
  fs.writeFileSync(path.join(dir,name+'.png'),Buffer.from(png,'base64'));
 }
 ws.close();console.log('Exported scroll + portrait PNG.');
})().catch(e=>{console.error(e);process.exit(1)});
