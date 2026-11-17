import test from 'node:test';
import assert from 'node:assert/strict';
import {meterAngle,vuAngle} from '../src/vu-meter.js';
const near=(a,b)=>assert.ok(Math.abs(a-b)<1e-9,`${a} != ${b}`);
test('needle crosses each printed dB tick using its actual pivot',()=>{
 for(const [db,x,y] of [[-20,49,81],[-10,76,71],[-7,101,63],[-5,125,57],[-3,151,54],[-2,175,52],[-1,199,54],[0,221,57],[1,245,63],[2,269,72],[3,293,86]]){
  const angle=vuAngle(db)*Math.PI/180;
  near(Math.tan(angle),(x-180)/(192-y));
 }
});
test('GR shows actual attenuation and moves left as compression increases',()=>{
 for(const reduction of [0,1,2,3,5,7,10,20])near(meterAngle('GR',[-12,-18,reduction]),vuAngle(-reduction));
 assert.ok(meterAngle('GR',[0,0,3])<meterAngle('GR',[0,0,1]));
 near(meterAngle('GR',[0,0,12],true),vuAngle(0));
});
test('IN and OUT are independent with -18 dBFS RMS = 0 VU',()=>{
 near(meterAngle('IN',[-18,-28,10]),vuAngle(0));
 near(meterAngle('OUT',[-18,-28,10]),vuAngle(-10));
 near(meterAngle('IN',[-18,-10,2]),vuAngle(0));
 near(meterAngle('OUT',[-18,-10,2]),vuAngle(3));
});
test('silence, overload and invalid readings cannot leave the meter scale',()=>{
 near(meterAngle('IN',[-120,-120,0]),vuAngle(-20));
 near(meterAngle('GR',[0,0,100]),vuAngle(-20));
 near(meterAngle('GR',[0,0,NaN]),vuAngle(0));
 near(meterAngle('OUT',null),vuAngle(-20));
});
