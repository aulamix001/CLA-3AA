import {readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const types={'.svg':'image/svg+xml','.ttf':'font/ttf','.woff2':'font/woff2','.png':'image/png'};
export async function bundleNativeUi(directory){
 const root=path.resolve(directory);
 const local=reference=>{
  if(!reference.startsWith('/')||reference.startsWith('//')||reference.includes('?')||reference.includes('#'))throw new Error(`Unsupported native resource: ${reference}`);
  const resolved=path.resolve(root,'.'+reference);
  if(!resolved.startsWith(root+path.sep))throw new Error('Native resource escapes build directory');
  return resolved;
 };
 let html=await readFile(path.join(root,'index.html'),'utf8');
 const scripts=[...html.matchAll(/<script\b[^>]*src="([^"]+)"[^>]*><\/script>/g)];
 const styles=[...html.matchAll(/<link\b[^>]*rel="stylesheet"[^>]*href="([^"]+)"[^>]*>/g)];
 if(scripts.length!==1||styles.length!==1)throw new Error('Expected one bundled entry script and stylesheet');
 let css=await readFile(local(styles[0][1]),'utf8');
 const urlPattern=/url\(\s*(?:"([^"]*)"|'([^']*)'|([^)]*))\s*\)/g;
 const refs=[...css.matchAll(urlPattern)].map(m=>({full:m[0],url:(m[1]??m[2]??m[3]).trim()}));
 for(const {full,url} of refs){
  if(url.startsWith('data:'))continue;
  const mime=types[path.extname(url)];if(!mime)throw new Error(`Unsupported native asset type: ${url}`);
  const data=await readFile(local(url));
  css=css.replace(full,()=>`url("data:${mime};base64,${data.toString('base64')}")`);
 }
 const js=await readFile(local(scripts[0][1]),'utf8');
 // No external imports are allowed: file:// module imports would reintroduce CORS.
 if(/\b(?:import\s*(?:\(|["'{*])|export\s)/.test(js))throw new Error('Native entry must be fully bundled');
 html=html.replace(styles[0][0],()=>'<style>'+css.replace(/<\/style/gi,'<\\/style')+'</style>');
 html=html.replace(scripts[0][0],'');
 html=html.replace('</body>',()=>'<script>'+js.replace(/<\/script/gi,'<\\/script')+'</script>\n</body>');
 await writeFile(path.join(root,'native.html'),html);
 return html;
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)){
 const html=await bundleNativeUi(process.argv[2]||'dist');
 console.log(`Native offline interface: ${Buffer.byteLength(html)} bytes, scripts/styles/fonts/textures embedded`);
}
