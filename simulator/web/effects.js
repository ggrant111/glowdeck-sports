const effectNames=['GOAL','POWER PLAY','PERIOD START','PERIOD END','GAME END','OVERTIME','SHOOTOUT'];
function effectPixel(e,color,i,count,ms){
 let level=255,t=Math.floor(ms/50);
 switch(e){
 case 0:level=(i+t*2)%24<8?255:45;break;
 case 1:level=(i+t)%32<10?220:30;break;
 case 2:level=i<=Math.floor(t*count/80)?230:20;break;
 case 3:level=255-Math.min(255,Math.floor(ms*255/8000));break;
 case 4:level=(i*13+t*3)%47<15?255:70;break;
 case 5:let phase=t%40;level=45+(phase<20?phase:40-phase)*10;break;
 case 6:level=(Math.floor(i/12)+Math.floor(t/12))%2?240:35;break;
 }
 return (Math.floor(((color>>16)&255)*level/255)<<16)|(Math.floor(((color>>8)&255)*level/255)<<8)|Math.floor((color&255)*level/255);
}
let effectFrame=0,effectActive=false,effectToken=0;
function stopEffectPreview(){effectToken++;cancelAnimationFrame(effectFrame);effectActive=false;const c=document.getElementById('effectStrip');if(c)c.getContext('2d').clearRect(0,0,c.width,c.height);}
function demoEffect(kind,home=false,preview=true){
 if(!s.online||effectActive||s.pending||s.active){log('Effect blocked: offline or busy');return;}
 if(!preview&&s.gameState&&s.gameState!=='LIVE'){log('Waiting for game start');return;}
 if(!preview&&(!s.armed||!s.ddp||(s.effectMask&(1<<kind))===0)){log('Event ignored: disarmed / DDP off / event disabled');return;}
 if(!preview&&(kind===0||kind===1)&&s.mode!=='both'&&s.mode!==(home?'home':'away')){log('Team event ignored');return;}
 if(!preview&&Date.now()<(s.cooldownUntil||0)){log('Cooldown: event ignored');return;}
 if(preview&&s.armed){log('Disarm before manual previews');return;}
 const token=++effectToken,wait=preview?0:s.delay*1000;s.pending=true;
 log(effectNames[kind]+': '+(preview?'manual preview':wait/1000+'s TV delay'));
 t=setTimeout(()=>{
 if(token!==effectToken||!s.online||(!preview&&!s.armed))return;
 s.pending=false;s.active=true;effectActive=true;s.cooldownUntil=Date.now()+20000;
 const start=performance.now(),color=home?0xffb81c:0x6cace4,canvas=document.getElementById('effectStrip'),ctx=canvas.getContext('2d');
 if(kind===0&&!preview&&s.audio)playSound(home?'BOS':'UTA');
 function frame(now){
 if(token!==effectToken)return;
 const ms=now-start;if(ms>=8000){cancel();render();log('DDP mock stopped; original scene restored');return;}
 ctx.clearRect(0,0,canvas.width,canvas.height);
 for(let i=0;i<60;i++){ctx.fillStyle='#'+effectPixel(kind,color,i,60,ms).toString(16).padStart(6,'0');ctx.fillRect(i*8,4,6,28);}
 effectFrame=requestAnimationFrame(frame);
 }
 effectFrame=requestAnimationFrame(frame);render();
 },wait);render();
}
function effectsControls(){
 const box=document.getElementById('effectsControls');box.innerHTML='';
 const mode=document.createElement('button');mode.textContent=s.ddp?'DDP ON':'DDP OFF';mode.onclick=()=>{if(s.armed||s.active||s.pending)return;s.ddp=!s.ddp;effectsControls()};box.appendChild(mode);
 effectNames.forEach((name,kind)=>{
 const row=document.createElement('div'),toggle=document.createElement('button');toggle.textContent=name+((s.effectMask&(1<<kind))?' ON':' OFF');
 toggle.onclick=()=>{if(s.armed||s.active||s.pending)return;s.effectMask^=1<<kind;effectsControls()};row.appendChild(toggle);
 for(const home of [false,true]){const b=document.createElement('button');b.textContent='Preview '+(home?'BOS':'UTA');b.onclick=()=>demoEffect(kind,home,true);row.appendChild(b);}
 const trigger=document.createElement('button');trigger.textContent='Simulate event';trigger.onclick=()=>demoEffect(kind,s.mode==='home',false);row.appendChild(trigger);box.appendChild(row);
 });
}
