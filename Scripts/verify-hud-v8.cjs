const fs=require('fs'),path=require('path');
const root=path.resolve(__dirname,'..'),out=path.join(root,'Docs/assets/hud-v8');
const wait=ms=>new Promise(r=>setTimeout(r,ms));
(async()=>{
const tabs=await(await fetch('http://127.0.0.1:9338/json')).json();
const ws=new WebSocket(tabs.find(t=>t.type==='page').webSocketDebuggerUrl);await new Promise(r=>ws.onopen=r);
let seq=0;const pending=new Map(),errors=[];
ws.onmessage=e=>{const d=JSON.parse(e.data);if(d.id){const p=pending.get(d.id);pending.delete(d.id);d.error?p.reject(d.error):p.resolve(d.result);}else if(d.method==='Runtime.exceptionThrown')errors.push(d.params.exceptionDetails);};
const call=(method,params={})=>new Promise((resolve,reject)=>{const id=++seq;pending.set(id,{resolve,reject});ws.send(JSON.stringify({id,method,params}));});
const ev=async expression=>(await call('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true})).result.value;
await call('Runtime.enable');await call('Page.enable');
await call('Emulation.setDeviceMetricsOverride',{width:1920,height:1080,deviceScaleFactor:1,mobile:false});
await call('Page.navigate',{url:'file:///'+path.join(root,'Docs/assets/hud-prototype-v8.0.html').replaceAll('\\','/')});await wait(700);
const checks=[];
for(const key of ['1','2','3','4','5','6','7','8','9','0','-','=']){
 await ev(`setState(${JSON.stringify(key)})`);await wait(['3','4','5'].includes(key)?1700:key==='-'?1100:160);
 const data=await ev(`({state:S.id,form:S.form,hp:S.hp1,ap:S.ap,charge:document.querySelector('#roMain').textContent,skills:document.querySelectorAll('.skill').length,images:[...document.images].every(i=>i.complete&&i.naturalWidth>0),width:document.querySelector('#viewport').getBoundingClientRect().width,p2:document.querySelector('#cf2').getBoundingClientRect().width/document.querySelector('#cf2').parentElement.getBoundingClientRect().width})`);
 checks.push({key,...data});
 if(['1','6','7','0','-'].includes(key)){const s=await call('Page.captureScreenshot',{format:'png'});fs.writeFileSync(path.join(out,'preview-'+({'-':'low','0':'limited'}[key]||key)+'.png'),Buffer.from(s.data,'base64'));}
}
const healthChecks=[];
for(const [hp,expected] of [[1000,'healthy'],[500,'caution'],[300,'danger'],[0,'danger']]){
 await ev(`setState('1');S.hp1=${hp};render()`);await wait(160);
 const actual=await ev(`document.querySelector('#idP1').dataset.health`);
 healthChecks.push({hp,expected,actual,pass:actual===expected});
 if(hp===500){await wait(950);const s=await call('Page.captureScreenshot',{format:'png'});fs.writeFileSync(path.join(out,'preview-half.png'),Buffer.from(s.data,'base64'));}
}
checks.push({healthChecks});
await ev(`setState('1')`);await wait(160);
const strip=await call('Page.captureScreenshot',{format:'png',clip:{x:0,y:20,width:1920,height:140,scale:1}});
fs.writeFileSync(path.join(out,'preview-resource-strip.png'),Buffer.from(strip.data,'base64'));
await ev(`setState('1');window.dispatchEvent(new KeyboardEvent('keydown',{key:'e'}))`);
checks.push({keyboardE:await ev(`S.form==='ranged'&&document.querySelector('[data-od-id="hud-skill-blast"]')!==null`)});
await ev(`document.querySelector('#reviewToggle').click()`);checks.push({panel:await ev(`getComputedStyle(document.querySelector('#panel')).display!=='none'`)});
await call('Emulation.setDeviceMetricsOverride',{width:1280,height:720,deviceScaleFactor:1,mobile:false});await wait(150);
checks.push({scaled:await ev(`Math.round(document.querySelector('#viewport').getBoundingClientRect().width)===1280`)});
fs.writeFileSync(path.join(out,'verification.json'),JSON.stringify({browser:'Microsoft Edge',errors,checks},null,2));
console.log(JSON.stringify({errors,checks},null,2));ws.close();
})().catch(e=>{console.error(e);process.exitCode=1});
