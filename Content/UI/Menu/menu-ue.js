// Local UE bridge. The approved prototype remains the source of the visuals.
(() => {
 let ready=false;
 const send=(action,details={})=>{
  if(!ready || !window.ue?.menu?.command) return;
  return window.ue.menu.command(JSON.stringify({action,...details}));
 };
 const originalGo=go;
 go=function(next,focusId){
  originalGo(next,focusId);
  send('screen',{screen:next});
 };
 const originalApply=applySettings;
 applySettings=function(showToast=true){
  originalApply(showToast);
  send('apply',{settings:clone(state.saved)});
 };
 const originalField=setField;
 setField=function(key,value){
  originalField(key,value);
  if(key==='brightness')send('brightness',{value});
 };
 document.addEventListener('click',e=>{
  const b=e.target.closest('button');
  if(!b||b.disabled)return;
  if(b.dataset.action==='enter-battle')send('start',{mode:state.mode,opponent:state.saved.opponent});
  if(b.dataset.modal==='confirm'&&state.modal){
   if(state.modal.type==='restart')send('restart');
   if(state.modal.type==='quit')send('quit');
  }
 },true);
 document.addEventListener('contextmenu',e=>e.preventDefault());
 window.menuUE={
  boot(settings){
   if(ready)return;
   if(!window.ue?.menu?.command){setTimeout(()=>this.boot(settings),100);return;}
   // Unreal's saved settings take precedence over the browser's preview storage.
   state.saved={...clone(DEFAULTS),...settings};
   ready=true;
   send('ready');
  },
  sync(settings){
   state.saved={...clone(DEFAULTS),...settings};
   if(state.screen==='settings'&&state.draft){
    state.draft=clone(state.saved);state.settingsBaseline=clone(state.saved);
    document.getElementById('settings-panel').innerHTML=settingsBody();
    updateDirty();updateBrightness();
   }
  },
  open(page,tab='graphics',opponent='静止木桩'){
   state.battleOpponent=opponent;
   // Keep the selected session type: free training may also use an AI opponent.
   if(page==='settings')openSettings(tab,'pause');
   else if(page==='controls'){state.controlsSource='pause';go(page);}
   else go(page);
  },
  back(){back();},
  scene(uri){const scene=document.querySelector('.scene.battle');if(scene)scene.src=uri;},
  probe(){send('probe',{value:JSON.stringify(window.menuPrototype.getState())});}
 };
})();
