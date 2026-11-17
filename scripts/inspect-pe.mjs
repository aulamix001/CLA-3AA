export function inspectPe(bytes){
 if(bytes.length<64||bytes.readUInt16LE(0)!==0x5a4d)throw new Error('Missing PE DOS header');
 const pe=bytes.readUInt32LE(0x3c);
 if(bytes.readUInt32LE(pe)!==0x4550||bytes.readUInt16LE(pe+24)!==0x20b)throw new Error('Expected PE32+ binary');
 const count=bytes.readUInt16LE(pe+6),optionalSize=bytes.readUInt16LE(pe+20),table=pe+24+optionalSize;
 const sections=Array.from({length:count},(_,i)=>{const p=table+i*40;return {va:bytes.readUInt32LE(p+12),size:Math.max(bytes.readUInt32LE(p+8),bytes.readUInt32LE(p+16)),raw:bytes.readUInt32LE(p+20)};});
 const offset=rva=>{const section=sections.find(s=>rva>=s.va&&rva<s.va+s.size);if(!section)throw new Error('Invalid PE RVA');return section.raw+rva-section.va;};
 const stringAt=rva=>{const p=offset(rva),end=bytes.indexOf(0,p);if(end<0)throw new Error('Unterminated PE string');return bytes.toString('ascii',p,end);};
 const imports=[],importRva=bytes.readUInt32LE(pe+24+120);
 if(importRva){let p=offset(importRva);while(bytes.readUInt32LE(p)||bytes.readUInt32LE(p+12)){imports.push(stringAt(bytes.readUInt32LE(p+12)));p+=20;}}
 const exports=[],exportRva=bytes.readUInt32LE(pe+24+112);
 if(exportRva){const p=offset(exportRva),n=bytes.readUInt32LE(p+24),names=offset(bytes.readUInt32LE(p+32));for(let i=0;i<n;++i)exports.push(stringAt(bytes.readUInt32LE(names+i*4)));}
 return {machine:bytes.readUInt16LE(pe+4),imports,exports};
}
