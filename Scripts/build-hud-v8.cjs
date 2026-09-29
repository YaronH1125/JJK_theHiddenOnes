const fs=require('fs'),path=require('path');
const root=path.resolve(__dirname,'..'), dir=path.join(root,'Docs/assets'), out=path.join(dir,'hud-v8');
fs.mkdirSync(out,{recursive:true});
const solid=d=>`<path d="${d}" fill="currentColor" stroke="none"/>`;
const line=d=>`<path d="${d}" fill="none" stroke="currentColor" stroke-width="1.25" stroke-linecap="round" stroke-linejoin="round"/>`;
const icons={
 punch:solid('M5 12V8l2-1 2 1V6l3-1 2 1 3-1 2 2v7l-4 5H8l-4-4-2-2 1-2z')+line('M8 9v3m4-5v4m4-4v4').replace('currentColor','#12212a')+line('M3 6 1 5m18 1 2-2m-1 9 2-1'),
 heavy:solid('M4 12V7l3-1 2 1V5l3-1 3 1 3-1 3 3v8l-5 5H7l-5-5V12z')+line('M8 8v4m4-6v5m4-5v5').replace('currentColor','#12212a')+solid('M2 3 5 4 3 6zm17-1 3 1-1 3z'),
 kick:solid('M7 3h6l1 6 4 5 4 1-1 4-6-1-6-7-2 9H3l2-11z')+line('M15 4c4 1 6 4 7 7M16 7l3 2'),
 hkick:solid('M7 3h6l1 6 4 5 4 1-1 4-6-1-6-7-2 9H3l2-11z')+solid('M18 3v3l3-1-2 3 3 2-4 1-3-5z')+line('M2 22h20'),
 swap:solid('M8 8V5Q12 0 16 5v3l-2 2h-4zM3 9l4 3-4 3v-2H1v-2h2zm18 2-4 3 4 3v-2h2v-2h-2z')+line('M6 5a9 9 0 0 1 14 3M18 20A9 9 0 0 1 4 17')+solid('M9 13h6l1 6-4 3-4-3z'),
 blast:solid('M9 21V15l-3-3 3-4h6l3 4-3 3v6zM11 7 10 2h4l-1 5zM6 8 3 4l-1 3 4 3zm12 0 3-4 1 3-4 3z')+line('M8 16h8m-7 3h6').replace('currentColor','#12212a')+solid('M11 10h2l2 2-3 2-3-2z').replace('currentColor','#12212a'),
 sblast:solid('M8 22V16l-4-4 3-5h10l3 5-4 4v6zM10 6 9 1h6l-1 5zM5 8 1 3v6l3 2zm14 0 4-5v6l-3 2zM4 15 1 18l5-1zm16 0 3 3-5-1z')+solid('M12 8 16 12 12 16 8 12z').replace('currentColor','#12212a')+solid('M12 10 14 12 12 14 10 12z')+line('M9 19h6').replace('currentColor','#12212a'),
 aim:line('M3 9V3h6m6 0h6v6m0 6v6h-6m-6 0H3v-6M12 6v3m0 6v3M6 12h3m6 0h3')+'<circle cx="12" cy="12" r="2" fill="currentColor" stroke="none"/>',
 domain:solid('M12 2 14 8 20 5 17 11 23 13 16 15 19 21 13 17 10 23 9 16 2 19 6 13 1 9 8 9z')+solid('M12 8 16 12 12 16 8 12z').replace('currentColor','#12212a')+solid('M12 10 14 12 12 14 10 12z')+line('M3 4 5 6m15-5-2 4M2 22l3-2'),
 mouse:line('M12 3c4 0 6 3 6 6v6a6 6 0 0 1-12 0V9c0-3 2-6 6-6Zm0 0v7m-6 0h12')+solid('M11 4C8 4 7 6 7 9h4z')
};
for(const [name,body] of Object.entries(icons))fs.writeFileSync(path.join(out,name+'.svg'),`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" color="#f2f5ee">${body}</svg>`);
let html=fs.readFileSync(path.join(dir,'hud-prototype-v7.0.html'),'utf8');
html=html.replace(/<title>.*?<\/title>/,'<title>石流龙 · 对战 HUD v8.0</title>');
html=html.replace(/const ICON = \{[\s\S]*?\n\};/,'const ICON = '+JSON.stringify(icons,null,2)+';');
const portrait=path.join(out,'ishigori-portrait.png');
if(fs.existsSync(portrait))html=html.replace(/data:image\/jpeg;base64,[A-Za-z0-9+/=]+/g,'data:image/png;base64,'+fs.readFileSync(portrait).toString('base64'));
const css=`
/* v8 visual layer. Geometry changes are documented in Docs/14. */
:root{--fg:#f2f5ee;--t4:#abb9c6;--hpc1:#366345;--hpc2:#7bbd75;--hpc3:#93c87d;--hpc4:#63ad64;--hpc5:#3d824f;--curse1:#17405f;--curse2:#53a5d9;--curse3:#83c5ed;--curse4:#3793ce;--curse5:#235f91}
#topband{height:200px;background:linear-gradient(#050b10ed,#060c1477 55%,transparent)}
.ident{top:38px;gap:0;align-items:flex-start}#idP1{left:44px}#idP2{right:44px}
.portrait{width:108px;height:108px;z-index:2;clip-path:none;border-radius:50%;background:linear-gradient(135deg,#e0d6b0,#6c634c 45%,#bdab7b);padding:2px;box-shadow:0 0 0 3px #111713,0 3px 8px #0009}
.portrait .inner,.portrait .imgwrap{clip-path:none;border-radius:50%}.portrait img{transform:none;filter:none;object-fit:cover}.portrait .imgwrap:after{background:linear-gradient(0deg,#050b1266,transparent 28%)}.portrait .sheen{display:none}.portrait .tag{font-size:11px;padding:1px 7px;bottom:-3px;left:18px}.portrait .edge{display:none}#idP2 .portrait .tag{right:18px}
#idP2 .portrait img{transform:scaleX(-1)}.portrait .edge{width:3px}
.stack{width:490px;padding:7px 0 0 12px;margin-left:-2px}#idP2 .stack{padding:7px 12px 0 0;margin:0 -2px 0 0}
.namerow{height:27px}.namerow .nm{font-size:23px;letter-spacing:.10em;font-family:KaiTi,STKaiti,serif;font-style:italic;color:#fff9e9;text-shadow:1px 2px #10110d,-1px -1px #10110d}.namerow .lvl{font-size:10px;background:transparent;box-shadow:none;border:0}.namerow .st{font-size:10px;background:transparent;box-shadow:none;border:0;padding:0}.namerow .hpv b{font-size:17px}.namerow .hpv{font-size:11px;color:#aab8c4}
.hpwrap{height:19px;margin-top:4px}.hpbar,#idP2 .hpbar{height:19px;padding:2px;background:linear-gradient(#d8caa0,#a29162 40%,#66593c);box-shadow:none}.hpbar,.cbar{clip-path:polygon(11px 0,100% 0,calc(100% - 11px) 100%,0 100%)}#idP2 .hpbar,#idP2 .cbar{clip-path:polygon(0 0,calc(100% - 11px) 0,100% 100%,11px 100%)}.hpclip,#idP2 .hpclip,.cbar>.b,#idP2 .cbar>.b{clip-path:inherit;background:#202721;box-shadow:inset 0 1px 2px #000b}
.hpclip .fill,#idP2 .hpclip .fill{background:linear-gradient(180deg,var(--hpc1),var(--hpc2) 24%,var(--hpc3) 40%,var(--hpc4) 67%,var(--hpc5));box-shadow:inset 0 1px #c6dd9b66,inset 0 -1px #1f482b99}.hpclip .ghost{background:#b65b48;box-shadow:none}.hpclip .lead{display:none}.hpclip .sheen{display:none}
.ident[data-health="caution"]{--hpc1:#87712d;--hpc2:#dac64f;--hpc3:#e5d76c;--hpc4:#c5b443;--hpc5:#95822d}.ident[data-health="danger"]{--hpc1:#7b2624;--hpc2:#d85342;--hpc3:#ea7961;--hpc4:#ca4737;--hpc5:#923128}.ident[data-health="danger"] .hpbar{filter:drop-shadow(0 0 3px #ae342c55)}
.cbar,#idP2 .cbar{height:13px;padding:2px;background:linear-gradient(#b6a875,#d4c69c 38%,#61573c);box-shadow:none}.cbar>.b,#idP2 .cbar>.b{background:#152128}.cbar .cf{box-shadow:inset 0 1px #a1d4ee66,inset 0 -1px #12375488}.cwrap .cmin{top:3px;bottom:3px;width:1px;background:#e4d29a;box-shadow:0 0 2px #000;opacity:.85}.cwrap .cmin:before,.cwrap .cmin:after{display:none}body.dry .cwrap .cmin{background:var(--danger);box-shadow:0 0 5px var(--danger)}
.aprow{margin-top:5px;height:22px;gap:8px}.aplab,.apval{font-size:10px}.pips{gap:4px;margin-left:0;padding:4px 7px;background:#101913;border:1px solid #8d805b;border-radius:12px;height:21px}.pip,.pip.off{width:26px;height:10px;transform:skewX(-12deg);border-radius:6px;background:#9a6936;box-shadow:none;overflow:hidden}.pip>i{inset:1px;border-radius:5px;background:linear-gradient(#c77b2b,#f9bd4e 48%,#e59c35);box-shadow:none}.pip>i:after{display:none}.pip.off{background:#544b32}.pip.off>i{background:#242b22;box-shadow:none}
.skill .orb{background:linear-gradient(135deg,#d6e2e77a,#bdcbd02a 45%,#e7f2f155);box-shadow:0 4px 15px #0006}.skill .disc{background:radial-gradient(at 35% 20%,#66757855,#101b227d 75%);box-shadow:inset 0 1px #fff2}.skill svg.ic{width:44px;height:44px;filter:drop-shadow(0 2px 2px #0009);stroke-width:1.2;color:#f2f5ee}
.skill .key,.ult .key{background:#eff2e9;color:#17212a;border:1px solid #fff8;border-radius:3px;clip-path:none;min-width:25px;line-height:21px;font-size:13px;box-shadow:0 2px 6px #0005;bottom:-15px}.skill .key.keyicon svg{width:17px;height:17px}.skill.basic .orb{background:transparent;box-shadow:none}.skill.basic .disc{background:transparent;box-shadow:none}
.ult .orb{background:linear-gradient(135deg,#d4ebeb99,#708c9744)}.ult .disc{background:radial-gradient(at 35% 25%,#66889055,#101b2288)}.ult svg.ic{width:54px;height:54px}.ult.ready .disc{background:radial-gradient(at 35% 25%,#43899599,#08202ddd)}.ult.ready .key{color:#103843;background:#dffcff}.skill::after{content:attr(data-n);position:absolute;top:-25px;left:-25px;right:-25px;text-align:center;font-size:12px;letter-spacing:.12em;color:#cfdbdf;opacity:0;transition:opacity .15s}.skill:hover::after{opacity:1}
#skills::before{content:'术 式  /  TECHNIQUES';position:absolute;left:1440px;top:911px;color:#c9d9dc88;font:10px var(--zh);letter-spacing:3px}
#formDisc{background:radial-gradient(#20393c88,#0a111766);box-shadow:none;border:1px solid #cfdbdf55}#formDisc svg{width:42px;height:42px}#cannon{opacity:0;transition:opacity .2s}body.charging #cannon{opacity:1}#readout{opacity:0}body.charging #readout,body.inspect #readout{opacity:1}#readout .bd{background:transparent;box-shadow:none}#readout:before{display:none}.ult.ready svg.ic{color:var(--deng3)}.ult.locked svg.ic{color:#82919b}
.modebar{top:168px;opacity:.75}.modebar b{background:#14232a66;border-color:#dce9df22;box-shadow:none}.modebar b.on{color:#f1ddb3;border-color:#d7b77466;background:#8d71451c}
#panel{display:none;z-index:30;top:250px;background:#90acb655}body.inspect #panel{display:block}#panel .bd{background:#09151af5}#panel button{font-size:12px;padding:7px}#panel .note{font-size:10px}
#keys{opacity:.6}#reviewToggle{position:absolute;left:44px;bottom:40px;z-index:35;border:1px solid #c5d7db44;border-radius:3px;background:#0a171bbf;color:#d5e1e4;padding:11px 16px;font:12px var(--zh);cursor:pointer}#reviewToggle:hover{background:#27414c}
.artmark{position:absolute;left:44px;bottom:94px;color:#c0d2d16b;font:10px var(--zh);letter-spacing:3px;pointer-events:none}
#arena .sky{background:radial-gradient(ellipse at 50% 42%,#365052,#142529 55%,#081316)}#arena .dome,#arena .dome2,#arena .plate,#arena .plate2,#arena .plate3{display:none}#arena .dojo{position:absolute;inset:0;width:100%;height:100%;opacity:.62}#arena .me,#arena .foe{opacity:.35}#arena .me{left:43%;top:55%}#arena .foe{left:53%;top:40%}
@media(prefers-reduced-motion:reduce){*,*:before,*:after{animation:none!important;transition:none!important}}
`;
html=html.replace('</style>',css+'\n</style>');
const dojo=`<svg class="dojo" viewBox="0 0 1920 1080" aria-hidden="true"><defs><linearGradient id="floor8" x2="0" y2="1"><stop stop-color="#1c3032"/><stop offset="1" stop-color="#40514e"/></linearGradient><radialGradient id="haze8"><stop stop-color="#a8c4ba" stop-opacity=".15"/><stop offset="1" stop-color="#a8c4ba" stop-opacity="0"/></radialGradient></defs><path d="M0 560H1920V1080H0Z" fill="url(#floor8)"/><path d="M0 559H1920M0 656H1920M0 794H1920M0 1000H1920M960 558 50 1080M960 558 540 1080M960 558 1380 1080M960 558 1870 1080" stroke="#b3c2b0" stroke-opacity=".12" fill="none"/><path d="M170 290 960 185 1750 290 1670 322 250 322Z" fill="#0b181b"/><path d="M215 332H1705V555H215Z" fill="#102124" stroke="#67776b" stroke-opacity=".25"/><path d="M270 335V555M485 335V555M700 335V555M915 335V555M1130 335V555M1345 335V555M1560 335V555" stroke="#0a181c" stroke-width="18"/><path d="M230 410H1690M230 485H1690" stroke="#6b867b" stroke-opacity=".17"/><path d="M0 561H1920M130 584H1790" stroke="#91aaa0" stroke-opacity=".18" stroke-width="3"/><ellipse cx="990" cy="546" rx="720" ry="400" fill="url(#haze8)"/><path d="M82 0V658M1838 0V658" stroke="#061115" stroke-width="55"/><path d="M0 217H1920" stroke="#061115" stroke-width="30"/></svg>`;
html=html.replace('<div class="sky"></div>','<div class="sky"></div>'+dojo);
html=html.replace('<div id="panel"','<span class="artmark">石流龙 · 道场 / HUD VISUAL STUDY 08</span><button id="reviewToggle" type="button" aria-expanded="false">状态预览 · Tab</button><div id="panel"');
html=html.replace('v2.0 交付稿 · 数值取自项目设计文档调试参数','v8.0 视觉原型 · 非引擎实机');
html=html.replace('const p1=S.hp1/MAX.hp*100, p2=S.hp2/MAX.hp*100;',`const p1=S.hp1/MAX.hp*100, p2=S.hp2/MAX.hp*100;
  [[1,p1],[2,p2]].forEach(([side,hp])=>document.getElementById('idP'+side).dataset.health=hp<=30?'danger':hp<=50?'caution':'healthy');`);
html=html.replace(/(<div id="keys"[^>]*>)[\s\S]*?(<\/div>)/,'$1<span>1–9 / 0 / − / =　状态预览　 ·　 E　切换形态　 ·　 右键　瞄准　 ·　 Tab　预览面板</span>$2');
html=html.replace("setState('1');",`document.getElementById('reviewToggle').onclick=()=>{document.body.classList.toggle('inspect');document.getElementById('reviewToggle').setAttribute('aria-expanded',document.body.classList.contains('inspect'));};
addEventListener('keydown',e=>{if(e.key==='Tab'){e.preventDefault();document.getElementById('reviewToggle').click();}});
setState('1');`);
fs.writeFileSync(path.join(dir,'hud-prototype-v8.0.html'),html);
fs.writeFileSync(path.join(out,'icons.json'),JSON.stringify(icons,null,2));
console.log('Built v8 prototype and '+Object.keys(icons).length+' SVG icons; portrait: '+fs.existsSync(portrait));
