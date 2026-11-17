import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,mkdir,writeFile,rm} from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import {bundleNativeUi} from '../scripts/bundle-native-ui.mjs';
async function fixture(t,css='body{background:url("/textures/metal.svg")}'){
 const directory=await mkdtemp(path.join(os.tmpdir(),'cla-native-test-'));t.after(()=>rm(directory,{recursive:true,force:true}));
 await mkdir(path.join(directory,'assets'));await mkdir(path.join(directory,'textures'));
 await writeFile(path.join(directory,'index.html'),'<html><head><script id="bootstrap">window.boot=true</script><script type="module" src="/assets/main.js"></script><link rel="stylesheet" href="/assets/main.css"></head><body><main>CLA-3A</main></body></html>');
 await writeFile(path.join(directory,'assets/main.js'),'document.querySelector("main").dataset.ready="true";');
 await writeFile(path.join(directory,'assets/main.css'),css);await writeFile(path.join(directory,'textures/metal.svg'),'<svg xmlns="http://www.w3.org/2000/svg"/>');
 return directory;
}
test('offline UI embeds scripts, stylesheet and texture and runs after controls exist',async t=>{
 const html=await bundleNativeUi(await fixture(t));
 assert.ok(html.includes('data:image/svg+xml;base64,'));
 assert.ok(html.indexOf('dataset.ready')>html.indexOf('<main>'));
 assert.ok(html.includes('window.boot=true'));
 assert.doesNotMatch(html,/<script[^>]+src=|<link[^>]+stylesheet|url\("\/textures/);
});
test('existing data SVG URLs with nested url() remain intact',async t=>{
 const css=`body{background:url("data:image/svg+xml,%3Csvg%3Eurl(%23noise)%3C/svg%3E")}`;
 const html=await bundleNativeUi(await fixture(t,css));assert.ok(html.includes(css));
});
test('missing assets fail packaging instead of leaving a blank editor',async t=>{
 await assert.rejects(bundleNativeUi(await fixture(t,'body{background:url(/textures/missing.svg)}')),/ENOENT/);
});
test('external resources cannot silently become network dependencies',async t=>{
 await assert.rejects(bundleNativeUi(await fixture(t,'body{background:url(https://example.com/metal.svg)}')),/Unsupported native resource/);
});
test('native UI refuses unbundled JavaScript imports',async t=>{
 const directory=await fixture(t);await writeFile(path.join(directory,'assets/main.js'),'import "./missing.js";');
 await assert.rejects(bundleNativeUi(directory),/fully bundled/);
});
