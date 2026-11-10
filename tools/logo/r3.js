const {chromium}=require('playwright');(async()=>{const b=await chromium.launch();
for(const [f,w,h,bgOmit] of [['splash_back',1000,860,1],['splash_glow',1000,860,1],['splash_front',1000,860,1],['splash_word',900,170,1],['logo_mark',400,360,1],['icon',512,512,0],['preview',1000,860,0]]){
const p=await b.newPage({viewport:{width:w,height:h},deviceScaleFactor:1});await p.goto('file://'+process.cwd()+'/'+f+'.svg');await p.screenshot({path:f+'.png',omitBackground:!!bgOmit});}await b.close()})()
