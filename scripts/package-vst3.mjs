import {inspectPe} from './inspect-pe.mjs';
import { readdir, mkdir, cp, copyFile, stat, readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const build = path.resolve(process.argv[2] || path.join(root,'build-vst3'));
async function bundles(folder) {
  const found=[];
  for(const entry of await readdir(folder,{withFileTypes:true})) {
    if(!entry.isDirectory())continue;
    const item=path.join(folder,entry.name);
    if(entry.name==='CLA-3A Studio.vst3')found.push(item);
    else if(!['_deps','CMakeFiles','package'].includes(entry.name))found.push(...await bundles(item));
  }
  return found;
}
const matches=await bundles(build);
if(matches.length!==1)throw new Error(`Expected one VST3 bundle, found ${matches.length}`);
const binary=path.join(matches[0],'Contents','x86_64-win','CLA-3A Studio.vst3');
const bytes=await readFile(binary);
if(bytes[0]!==0x4d||bytes[1]!==0x5a)throw new Error('Missing Windows PE binary');
const info=inspectPe(bytes);
if(!info.exports.includes('GetPluginFactory')||!info.exports.includes('InitDll'))throw new Error('VST3 scanner exports missing');
if(info.imports.some(name=>/^(MSVCP|VCRUNTIME)/i.test(name)))throw new Error('VST3 has external Visual C++ runtime dependencies');
const pe=bytes.readUInt32LE(0x3c);
if(bytes.readUInt16LE(pe+4)!==0x8664)throw new Error('VST3 must be Windows x64');
if((await stat(path.join(matches[0],'Contents','Resources','web','native.html'))).size<100)throw new Error('Missing approved interface');
const output=path.join(build,'package');await mkdir(output,{recursive:true});
await cp(matches[0],path.join(output,'CLA-3A Studio.vst3'),{recursive:true});
await copyFile(path.join(root,'INSTALL-VST3.txt'),path.join(output,'INSTALL-VST3.txt'));
const licenses=path.join(output,'Third-party licenses');await mkdir(licenses,{recursive:true});
await copyFile(path.join(build,'_deps','vst3sdk-src','LICENSE.txt'),path.join(licenses,'VST3-SDK-MIT.txt'));
await copyFile(path.join(build,'_deps','webview2-src','LICENSE.txt'),path.join(licenses,'WebView2.txt'));
console.log(`Verified Windows x64 VST3 with embedded interface: ${output}`);

const vst2=path.join(build,'Release','CLA-3A Studio.dll');
const legacy=await readFile(vst2);const legacyInfo=inspectPe(legacy);
if(!legacyInfo.exports.includes('VSTPluginMain')||legacyInfo.imports.some(name=>/^(MSVCP|VCRUNTIME)/i.test(name)))throw new Error('VST2 export/runtime verification failed');
const legacyPe=legacy.readUInt32LE(0x3c);
if(legacy[0]!==0x4d||legacy[1]!==0x5a||legacy.readUInt16LE(legacyPe+4)!==0x8664)throw new Error('VST2 must be Windows x64');
const vst2Package=path.join(build,'package-vst2');await mkdir(vst2Package,{recursive:true});
await copyFile(vst2,path.join(vst2Package,'CLA-3A Studio.dll'));
await cp(path.join(build,'Release','CLA-3A Studio Resources'),path.join(vst2Package,'CLA-3A Studio Resources'),{recursive:true});
await copyFile(path.join(root,'INSTALL-VST2.txt'),path.join(vst2Package,'INSTALL-VST2.txt'));
await cp(licenses,path.join(vst2Package,'Third-party licenses'),{recursive:true});
