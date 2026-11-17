import test from 'node:test';
import assert from 'node:assert/strict';
import {revealNativeInterface} from '../src/native-presentation.js';
const turn=()=>new Promise(resolve=>setImmediate(resolve));
function setup(complete=true){
 let fontsReady,loaded;
 const frames=[],messages=[];
 const doc={readyState:complete?'complete':'loading',fonts:{ready:new Promise(resolve=>fontsReady=resolve)}};
 const win={requestAnimationFrame:fn=>frames.push(fn),addEventListener:(name,fn)=>{assert.equal(name,'load');loaded=fn;}};
 return {frames,messages,doc,win,font:()=>fontsReady(),load:()=>loaded(),bridge:{postMessage:m=>messages.push(m)}};
}
test('cover reveal waits for resources, fonts and two paint opportunities',async()=>{
 const s=setup(false);const result=revealNativeInterface(s.bridge,s.doc,s.win);
 s.font();await turn();assert.equal(s.frames.length,0);assert.deepEqual(s.messages,[]);
 s.load();await turn();assert.equal(s.frames.length,1);
 assert.deepEqual(s.messages,['diagnostic:resources and fonts ready']);
 s.frames.shift()();await turn();assert.equal(s.frames.length,1);assert.ok(!s.messages.includes('painted'));
 s.frames.shift()();await result;assert.equal(s.messages.at(-1),'painted');
});
test('already loaded document still waits for fonts and the renderer',async()=>{
 const s=setup();const result=revealNativeInterface(s.bridge,s.doc,s.win);
 await turn();assert.equal(s.frames.length,0);s.font();await turn();
 s.frames.shift()();await turn();s.frames.shift()();await result;
 assert.equal(s.messages.filter(m=>m==='painted').length,1);
});
test('resource/font failure does not falsely announce a painted UI',async()=>{
 const s=setup();s.doc.fonts.ready=Promise.reject(new Error('fonts failed'));
 await assert.rejects(revealNativeInterface(s.bridge,s.doc,s.win),/fonts failed/);
 assert.ok(!s.messages.includes('painted'));
});
