const fs=require('fs'),path=require('path');
const root=path.resolve(__dirname,'..');
(async()=>{
const tabs=await(await fetch('http://127.0.0.1:9338/json')).json();const ws=new WebSocket(tabs.find(t=>t.type==='page').webSocketDebuggerUrl);await new Promise(r=>ws.onopen=r);
let id=0;const pending=new Map();ws.onmessage=e=>{const d=JSON.parse(e.data);if(d.id){pending.get(d.id)(d.result);pending.delete(d.id);}};
const call=(method,params={})=>new Promise(r=>{pending.set(++id,r);ws.send(JSON.stringify({id,method,params}));});
await call('Emulation.setDeviceMetricsOverride',{width:1440,height:1050,deviceScaleFactor:1,mobile:false});
await call('Page.navigate',{url:'file:///'+root.replaceAll('\\','/')+'/Docs/assets/od-hud-states-v8.0.html'});await new Promise(r=>setTimeout(r,800));
const result=await call('Runtime.evaluate',{expression:`({images:[...document.images].every(i=>i.complete&&i.naturalWidth>0),overflow:document.documentElement.scrollWidth>innerWidth,icons:document.querySelectorAll('.icon').length})`,returnByValue:true});
console.log(result.result.value);
for(const [name,y] of [['board-top',0],['board-assets',1320]]){await call('Runtime.evaluate',{expression:`scrollTo(0,${y})`});await new Promise(r=>setTimeout(r,100));const s=await call('Page.captureScreenshot',{format:'png'});fs.writeFileSync(path.join(root,'Docs/assets/hud-v8/'+name+'.png'),Buffer.from(s.data,'base64'));}
ws.close();
})().catch(e=>{console.error(e);process.exitCode=1});
