// Printed tick endpoints in the SVG, relative to its physical needle pivot.
// Interpolate in dB between ticks; a constant degrees/dB cannot match this scale.
export const VU_TICKS = Object.freeze([
  [-20,49,81],[-10,76,71],[-7,101,63],[-5,125,57],[-3,151,54],
  [-2,175,52],[-1,199,54],[0,221,57],[1,245,63],[2,269,72],[3,293,86],
].map(([db,x,y]) => Object.freeze({db,angle:Math.atan2(x-180,192-y)*180/Math.PI})));
export const VU_REFERENCE_DBFS = -18; // RMS dBFS: -18 dBFS = 0 VU.
export function vuAngle(db) {
  if(!Number.isFinite(db))return VU_TICKS[0].angle;
  if(db<=VU_TICKS[0].db)return VU_TICKS[0].angle;
  for(let i=1;i<VU_TICKS.length;i++) {
    const a=VU_TICKS[i-1],b=VU_TICKS[i];
    if(db<=b.db)return a.angle+(b.angle-a.angle)*(db-a.db)/(b.db-a.db);
  }
  return VU_TICKS.at(-1).angle;
}
export function meterAngle(mode, meters, bypass=false) {
  if(mode==='GR')return vuAngle(bypass?0:-Math.max(0,Number.isFinite(meters?.[2])?meters[2]:0));
  const level=meters?.[mode==='IN'?0:1];
  return vuAngle(Number.isFinite(level)?level-VU_REFERENCE_DBFS:-120);
}
