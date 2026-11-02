import fs from 'node:fs';
import vm from 'node:vm';
import {execFileSync} from 'node:child_process';
import assert from 'node:assert/strict';
const context=vm.createContext({});vm.runInContext(fs.readFileSync('simulator/web/effects.js','utf8'),context);
execFileSync('c++',['-std=c++17','-Wall','-Wextra','-Werror','-Iinclude','scripts/effect-vectors.cpp','-o','/tmp/gameday-effects-vectors']);
const lines=execFileSync('/tmp/gameday-effects-vectors',{encoding:'utf8'}).trim().split('\n');
for(const line of lines){const [e,ms,i,rgb]=line.split(' ').map(Number);assert.equal(vm.runInContext(`effectPixel(${e},0x6cace4,${i},600,${ms})`,context),rgb);}
const html=fs.readFileSync('simulator/web/index.html','utf8');new vm.Script(html.match(/<script>([\s\S]*?)<\/script>/)[1]);
console.log(`PASS ${lines.length} C++/browser pixel vectors; browser script parses`);
// Exercise the browser's actual handlers with deterministic timers and a minimal DOM.
const elements=new Map(),timers=new Map(),frames=new Map();let serial=0,now=0;
const element=()=>({textContent:'',innerHTML:'',style:{},children:[],appendChild(e){this.children.push(e)},getContext(){return {clearRect(){},fillRect(){}}}});
const document={getElementById(id){if(!elements.has(id))elements.set(id,element());return elements.get(id)},createElement:element};
const ui=vm.createContext({document,performance:{now:()=>now},Date,setTimeout(fn){const id=++serial;timers.set(id,fn);return id},clearTimeout(id){timers.delete(id)},requestAnimationFrame(fn){const id=++serial;frames.set(id,fn);return id},cancelAnimationFrame(id){frames.delete(id)}});
vm.runInContext(fs.readFileSync('simulator/web/effects.js','utf8'),ui);
vm.runInContext(html.match(/<script>([\s\S]*?)<\/script>/)[1],ui);
const run=s=>vm.runInContext(s,ui);
const fireTimer=()=>{const [id,fn]=timers.entries().next().value;timers.delete(id);fn()};
run('demoEffect(5,true,true)');assert.equal(run('s.pending'),true);fireTimer();assert.equal(run('s.active&&effectActive'),true);
now=8001;const [id,frame]=frames.entries().next().value;frames.delete(id);frame(now);assert.equal(run('s.active||s.pending||effectActive'),false);
run("reset();selectGame();chooseMode('both');s.ddp=true;arm();demoEffect(1,true,false)");fireTimer();assert.equal(run('s.active'),true);
run('disconnect()');assert.equal(run('s.active||s.pending||effectActive'),false);
run('reset();selectGame();s.ddp=true;arm();s.effectMask&=~(1<<5);demoEffect(5,false,false)');assert.equal(run('s.pending||s.active'),false);
run('demoEffect(1,true,false)');assert.equal(run('s.pending||s.active'),false); // HOME excluded in AWAY mode
console.log('PASS browser handlers: preview, automatic restore, power-play trigger, outage, disabled event, team filter');

run("reset();selectGame('FUT');s.ddp=true;arm();goal(false)");assert.equal(run('s.armed'),true);assert.equal(run('s.active||s.pending'),false);
run('s.gameState="LIVE";demoEffect(2,false,false)');assert.equal(run('s.pending'),true);fireTimer();assert.equal(run('s.active'),true);
run('s.resume=true;reset()');assert.equal(run('s.armed&&s.resume'),true);assert.equal(run('s.active||s.pending'),false);
run('arm();reset()');assert.equal(run('s.armed'),false);
run('s.page="lights";s.ledPage=2;render()');assert.match(elements.get('body').innerHTML,/WLED 8/);
console.log('PASS browser upcoming waiting/start, opt-in reboot resume, disarm and last device page');
