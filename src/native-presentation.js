// Keep the native loading cover until host state, resources, fonts and layout
// have reached the renderer. Browser initialization alone is not a painted UI.
export async function revealNativeInterface(bridge, doc=document, win=window) {
 if(doc.readyState!=='complete')await new Promise(resolve=>win.addEventListener('load',resolve,{once:true}));
 if(doc.fonts)await doc.fonts.ready;
 bridge.postMessage('diagnostic:resources and fonts ready');
 await new Promise(resolve=>win.requestAnimationFrame(resolve));
 await new Promise(resolve=>win.requestAnimationFrame(resolve));
 bridge.postMessage('painted');
}
