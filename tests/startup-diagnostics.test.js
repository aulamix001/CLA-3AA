import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';
const html=readFileSync(new URL('../index.html',import.meta.url),'utf8');
const bootstrap=html.match(/<script id="startup-diagnostics">([\s\S]*?)<\/script>/)[1];
function boot(native=true){
 const messages=[],handlers={};
 const sandbox={addEventListener:(name,fn)=>handlers[name]=fn};
 if(native)sandbox.chrome={webview:{postMessage:message=>messages.push(message)}};
 runInNewContext(bootstrap,sandbox);const global=runInNewContext("globalThis",sandbox);return {sandbox,messages,handlers,global};
}
test('startup reports before modules and identifies readiness stages',()=>{
 const {sandbox,messages,handlers}=boot();
 assert.deepEqual(messages,['diagnostic:HTML bootstrap running']);
 sandbox.__claStartup('main module entered');handlers.DOMContentLoaded();
 assert.ok(messages.includes('diagnostic:main module entered'));
 assert.ok(messages.includes('diagnostic:DOM content loaded'));
});
test('missing module resource identifies filename without query values',()=>{
 const {messages,handlers}=boot();
 handlers.error({target:{src:'https://cla3a.local/assets/missing.js?private=value'}});
 assert.equal(messages.at(-1),'startup-error:resource missing.js');
});
test('startup exceptions and rejected promises reach the native error path',()=>{
 const {messages,handlers,global}=boot();
 handlers.error({target:global,message:'test failure',lineno:42});
 assert.equal(messages.at(-1),'startup-error:test failure line 42');
 handlers.unhandledrejection();
 assert.equal(messages.at(-1),'startup-error:unhandled promise rejection');
});
test('normal browser use requires no WebView2 bridge',()=>{
 const {messages,handlers}=boot(false);
 assert.deepEqual(messages,[]);assert.deepEqual(handlers,{});
});

test('console errors and warnings reach native diagnostics without disabling console output',()=>{
 const {sandbox,messages}=boot();
 runInNewContext('console.error("missing main.js");console.warn("slow renderer")',sandbox);
 assert.ok(messages.includes('console-error:missing main.js'));
 assert.ok(messages.includes('console-warn:slow renderer'));
});
