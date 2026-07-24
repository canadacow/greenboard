# Assemble tune_monitor.html (moment-transport version): inlines assets as data URIs.
import base64
import os

D = os.path.dirname(os.path.abspath(__file__))

def b64(name, mime):
    with open(os.path.join(D, name), "rb") as f:
        return f"data:{mime};base64," + base64.b64encode(f.read()).decode()

ASSETS = {
    "@@PLATE@@": b64("plate_on.jpg", "image/jpeg"),
    "@@LED@@": b64("led_layer.jpg", "image/jpeg"),
    "@@WARP_A@@": b64("warp_a.png", "image/png"),
    "@@WARP_B@@": b64("warp_b.png", "image/png"),
    "@@D_MA@@": b64("diff_mean_a.png", "image/png"),
    "@@D_MB@@": b64("diff_mean_b.png", "image/png"),
    "@@D_CV@@": b64("diff_cov.png", "image/png"),
    "@@D_W@@": b64("diff_w.png", "image/png"),
    "@@G_MA@@": b64("gloss_mean_a.png", "image/png"),
    "@@G_MB@@": b64("gloss_mean_b.png", "image/png"),
    "@@G_CV@@": b64("gloss_cov.png", "image/png"),
    "@@G_W@@": b64("gloss_w.png", "image/png"),
    "@@SQ2@@": b64("sq2.png", "image/png"),
}

HTML = r"""<title>5151 Adjustment Console</title>
<style>
  /* Deliberate single-theme: CRT adjustment happens in a dim service bay. */
  :root{
    --bench:#16181a; --panel:#23272b; --panel-hi:#2a2f34; --line:#3a4046;
    --ink:#d8d3c6; --ink-dim:#8f8b80; --phos:#54e08a; --phos-dim:#2e7a4d;
    --amber:#e0a33c; --mono:ui-monospace,"Cascadia Mono",Consolas,monospace;
    --sans:"Segoe UI",system-ui,sans-serif;
  }
  html{background:var(--bench)}
  body{margin:0;color:var(--ink);font-family:var(--sans);font-size:14px}
  .rig{display:flex;gap:18px;padding:18px;max-width:1580px;margin:0 auto;align-items:flex-start;flex-wrap:wrap}
  .tube{flex:1 1 640px;min-width:480px}
  canvas{width:100%;display:block;background:#000;border:1px solid var(--line)}
  header{padding:14px 18px 0}
  h1{font-size:15px;letter-spacing:.24em;font-weight:600;margin:0;text-transform:uppercase}
  h1 small{color:var(--phos);letter-spacing:.24em}
  .sub{color:var(--ink-dim);font-family:var(--mono);font-size:11px;margin-top:4px}
  .rack{flex:0 1 380px;display:flex;flex-direction:column;gap:12px;min-width:320px}
  .grp{background:var(--panel);border:1px solid var(--line);padding:12px 14px}
  .grp .lg{font-size:10.5px;letter-spacing:.22em;text-transform:uppercase;
    color:var(--ink-dim);margin:0 0 10px;display:flex;justify-content:space-between;align-items:center}
  .lg button{background:none;border:1px solid var(--line);color:var(--ink-dim);
    font:10px var(--mono);padding:2px 8px;cursor:pointer;letter-spacing:.1em}
  .lg button:hover,.lg button:focus-visible{color:var(--phos);border-color:var(--phos-dim);outline:none}
  .ctl{display:grid;grid-template-columns:86px 1fr 62px;gap:10px;align-items:center;margin:7px 0}
  .ctl label{font-size:11px;letter-spacing:.08em;text-transform:uppercase;color:var(--ink)}
  .ctl output{font:12px var(--mono);color:var(--phos);text-align:right;font-variant-numeric:tabular-nums}
  input[type=range]{-webkit-appearance:none;appearance:none;height:22px;background:none;cursor:ew-resize}
  input[type=range]::-webkit-slider-runnable-track{height:3px;background:var(--line)}
  input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:9px;height:17px;margin-top:-7px;
    background:var(--ink);border:1px solid var(--bench);box-shadow:0 0 0 1px var(--line)}
  input[type=range]:focus-visible{outline:1px solid var(--phos);outline-offset:2px}
  .chips{display:flex;flex-wrap:wrap;gap:6px}
  .chips button{background:var(--panel-hi);border:1px solid var(--line);color:var(--ink);
    font:11px var(--mono);padding:5px 10px;cursor:pointer;letter-spacing:.05em}
  .chips button[aria-pressed="true"]{color:#101311;background:var(--phos);border-color:var(--phos)}
  .chips button:focus-visible{outline:1px solid var(--phos);outline-offset:2px}
  .exp pre{background:#101214;border:1px solid var(--line);color:var(--phos);
    font:11px/1.5 var(--mono);padding:10px;margin:0 0 8px;overflow-x:auto;max-height:180px}
  .exp .row{display:flex;gap:8px}
  .exp button{background:var(--panel-hi);border:1px solid var(--line);color:var(--ink);
    font:11px var(--mono);padding:6px 12px;cursor:pointer;letter-spacing:.08em}
  .exp button:hover{border-color:var(--phos-dim);color:var(--phos)}
  .note{color:var(--ink-dim);font-size:11px;line-height:1.5;margin-top:8px}
  .note em{color:var(--amber);font-style:normal}
  @media (prefers-reduced-motion: no-preference){
    .chips button,.exp button,.lg button{transition:color .12s,border-color .12s,background .12s}
  }
</style>
<header>
  <h1>IBM 5151 <small>// adjustment console</small></h1>
  <div class="sub">bench plate 3200&times;2400 &middot; per-lobe moment transport (Cycles-measured) &middot; live composite</div>
</header>
<div class="rig">
  <div class="tube"><canvas id="crt" width="1600" height="1200"></canvas></div>
  <div class="rack">

    <div class="grp"><div class="lg">Power</div>
      <div class="chips" id="pwr" role="group" aria-label="Power">
        <button data-w="on" aria-pressed="true">&#9679; ON</button>
        <button data-w="off" aria-pressed="false">OFF</button>
      </div>
    </div>

    <div class="grp"><div class="lg">Geometry <button data-reset="geo">reset</button></div>
      <div class="ctl"><label for="hsize">H size</label><input type="range" id="hsize" min="0.70" max="1.30" step="0.005" value="1.0"><output for="hsize">1.000</output></div>
      <div class="ctl"><label for="vsize">V size</label><input type="range" id="vsize" min="0.70" max="1.30" step="0.005" value="1.0"><output for="vsize">1.000</output></div>
      <div class="ctl"><label for="hpos">H center</label><input type="range" id="hpos" min="-0.12" max="0.12" step="0.002" value="0"><output for="hpos">+0.000</output></div>
      <div class="ctl"><label for="vpos">V center</label><input type="range" id="vpos" min="-0.12" max="0.12" step="0.002" value="0"><output for="vpos">+0.000</output></div>
    </div>

    <div class="grp"><div class="lg">Video <button data-reset="vid">reset</button></div>
      <div class="ctl"><label for="bright">Brightness</label><input type="range" id="bright" min="0" max="0.25" step="0.002" value="0.02"><output for="bright">0.020</output></div>
      <div class="ctl"><label for="contrast">Contrast</label><input type="range" id="contrast" min="0.2" max="2.5" step="0.01" value="1.0"><output for="contrast">1.00</output></div>
      <div class="ctl"><label for="gain">Screen gain</label><input type="range" id="gain" min="0.2" max="4" step="0.02" value="1.6"><output for="gain">1.60</output></div>
    </div>

    <div class="grp"><div class="lg">Phosphor</div>
      <div class="chips" id="phos" role="group" aria-label="Phosphor">
        <button data-p="color" aria-pressed="true">CGA COLOR</button>
        <button data-p="green" aria-pressed="false">P1 GREEN</button>
        <button data-p="amber" aria-pressed="false">P3 AMBER</button>
        <button data-p="white" aria-pressed="false">P4 WHITE</button>
      </div>
    </div>

    <div class="grp"><div class="lg">Room <button data-reset="room">reset</button></div>
      <div class="ctl"><label for="glow">Glow spill</label><input type="range" id="glow" min="0" max="3" step="0.02" value="1.0"><output for="glow">1.00</output></div>
      <div class="ctl"><label for="plate">Bench light</label><input type="range" id="plate" min="0.1" max="2" step="0.01" value="1.0"><output for="plate">1.00</output></div>
    </div>

    <div class="grp"><div class="lg">Test pattern</div>
      <div class="chips" id="pat" role="group" aria-label="Test pattern">
        <button data-t="sq2" aria-pressed="true">SQ2</button>
        <button data-t="hatch" aria-pressed="false">CROSSHATCH</button>
        <button data-t="dots" aria-pressed="false">DOTS</button>
        <button data-t="bars" aria-pressed="false">CGA BARS</button>
        <button data-t="steps" aria-pressed="false">GRAY STEPS</button>
        <button data-t="white" aria-pressed="false">WHITE</button>
      </div>
    </div>

    <div class="grp exp"><div class="lg">Export &middot; renderer.cpp</div>
      <pre id="json" aria-live="off"></pre>
      <div class="row"><button id="copy">COPY JSON</button><button id="factory">FACTORY RESET</button></div>
      <div class="note">Spill is measured light transport: per pixel and per lobe
      (diffuse / glossy), Cycles-baked weight, mean footprint, and covariance,
      evaluated as a 5-tap anisotropic gather over the live frame. No model knobs
      &mdash; <em>glow spill is the only trim</em>. Geometry maps the framebuffer into
      the tube's visible UV window (u 0.008&ndash;0.970, v 0.079&ndash;0.956).
      <em>Brightness lifts the whole raster, border included.</em></div>
    </div>

  </div>
</div>
<script>
"use strict";
const $=id=>document.getElementById(id);
const P={hsize:1,vsize:1,hpos:0,vpos:0,bright:0.02,contrast:1,gain:1.6,glow:1,plate:1,
         power:"on",phosphor:"color",pattern:"sq2"};
const DEF=JSON.parse(JSON.stringify(P));
const WIN={u0:0.008,u1:0.970,v0:0.079,v1:0.956};   // visible tube UV window (Blender v: 0=bottom)

const gl=$("crt").getContext("webgl2",{preserveDrawingBuffer:false,antialias:false});
const VS=`#version 300 es
in vec2 p;out vec2 tc;
void main(){tc=vec2(p.x*0.5+0.5,1.0-(p.y*0.5+0.5));gl_Position=vec4(p,0.,1.);}`;
const FS=`#version 300 es
precision highp float;in vec2 tc;out vec4 fragColor;
uniform sampler2D uPlate,uLed,uWarpA,uWarpB,uFrame,uFrameMip;
uniform sampler2D uDMa,uDMb,uDCv,uDW,uGMa,uGMb,uGCv,uGW;
uniform float uPower;
uniform vec4 uGeo;      // hsize, vsize, hpos, vpos
uniform vec4 uVid;      // bright, contrast, gain, plateExp
uniform vec4 uWin;      // u0,u1,v0,v1 (Blender v)
uniform float uGlowGain;
uniform vec4 uPhos;     // rgb tint, w: 1=mono
const float SIG_MAX=0.6;
vec3 lin(vec3 c){return pow(max(c,vec3(0.0)),vec3(2.2));}
float dec(vec3 t){return (t.r*255.0*256.0+t.g*255.0)/65535.0;}
vec2 fbmap(float u,float v){
  float xf=((u-uWin.x)/(uWin.y-uWin.x)-0.5-uGeo.z)/uGeo.x+0.5;
  float yf=((uWin.w-v)/(uWin.w-uWin.z)-0.5-uGeo.w)/uGeo.y+0.5;
  return vec2(xf,yf);
}
vec3 shade(vec3 content){
  float luma=dot(content,vec3(0.299,0.587,0.114));
  content=mix(content,uPhos.rgb*luma,uPhos.w);
  return (content*uVid.y+uVid.x)*uVid.z;
}
vec3 lobe(sampler2D ma,sampler2D mb,sampler2D cv,sampler2D wt){
  vec3 A=texture(ma,tc).rgb;
  if(A.b<0.5)return vec3(0.0);
  float mu=dec(A),mv=dec(texture(mb,tc).rgb);
  vec3 C=texture(cv,tc).rgb;
  float su=C.r*C.r*SIG_MAX,sv=C.g*C.g*SIG_MAX,rho=C.b*2.0-1.0;
  // covariance from screen-UV units into framebuffer units (affine geometry map)
  float jx=1.0/((uWin.y-uWin.x)*uGeo.x),jy=1.0/((uWin.w-uWin.z)*uGeo.y);
  float cuu=su*su*jx*jx,cvv=sv*sv*jy*jy,cuv=rho*su*sv*jx*jy;
  // closed-form 2x2 eigen: principal smear axis + widths
  float tr=cuu+cvv,df=cuu-cvv;
  float disc=sqrt(df*df*0.25+cuv*cuv);
  float l1=max(tr*0.5+disc,1e-12),l2=max(tr*0.5-disc,1e-12);
  vec2 ax=(abs(cuv)>1e-12)?normalize(vec2(cuv,l1-cuu))
                          :((cuu>=cvv)?vec2(1.0,0.0):vec2(0.0,1.0));
  float sMaj=sqrt(l1),sMin=sqrt(l2);
  vec2 fb0=fbmap(mu,mv);
  float px=sMin*length(vec2(800.0,600.0)*ax.yx);
  float lod=clamp(log2(max(px,1.0)),0.0,11.0);
  float W[5];W[0]=0.13;W[1]=0.23;W[2]=0.28;W[3]=0.23;W[4]=0.13;
  float T[5];T[0]=-1.6;T[1]=-0.8;T[2]=0.0;T[3]=0.8;T[4]=1.6;
  vec3 acc=vec3(0.0);
  for(int i=0;i<5;i++){
    vec2 f2=fb0+ax*sMaj*T[i];
    vec3 ct=vec3(0.0);
    if(all(greaterThanEqual(f2,vec2(0.0)))&&all(lessThanEqual(f2,vec2(1.0))))
      ct=lin(textureLod(uFrameMip,f2,lod).rgb);
    acc+=ct*W[i];
  }
  return lin(texture(wt,tc).rgb)*shade(acc);
}
void main(){
  vec3 a=texture(uWarpA,tc).rgb;
  float cov=step(0.5,a.b);
  // additive layers: studio plate scales with bench light; the LED's own
  // emission does not -- it only obeys the power switch
  vec3 c=lin(texture(uPlate,tc).rgb)*uVid.w+lin(texture(uLed,tc).rgb)*uPower;
  if(cov>0.5){
    float u=dec(a),v=dec(texture(uWarpB,tc).rgb);
    vec2 fb=fbmap(u,v);
    vec3 content=vec3(0.0);
    if(all(greaterThanEqual(fb,vec2(0.0)))&&all(lessThanEqual(fb,vec2(1.0))))
      content=lin(texture(uFrame,fb).rgb);
    c+=shade(content)*uPower;
  }else{
    c+=(lobe(uDMa,uDMb,uDCv,uDW)+lobe(uGMa,uGMb,uGCv,uGW))*uGlowGain*uPower;
  }
  fragColor=vec4(pow(max(c,vec3(0.0)),vec3(1.0/2.2)),1.0);
}`;
function sh(t,s){const o=gl.createShader(t);gl.shaderSource(o,s);gl.compileShader(o);
 if(!gl.getShaderParameter(o,gl.COMPILE_STATUS))throw gl.getShaderInfoLog(o);return o;}
const prog=gl.createProgram();
gl.attachShader(prog,sh(gl.VERTEX_SHADER,VS));
gl.attachShader(prog,sh(gl.FRAGMENT_SHADER,FS));
gl.linkProgram(prog);
if(!gl.getProgramParameter(prog,gl.LINK_STATUS))throw gl.getProgramInfoLog(prog);
gl.useProgram(prog);
gl.bindBuffer(gl.ARRAY_BUFFER,gl.createBuffer());
gl.bufferData(gl.ARRAY_BUFFER,new Float32Array([-1,-1,3,-1,-1,3]),gl.STATIC_DRAW);
const loc=gl.getAttribLocation(prog,"p");
gl.enableVertexAttribArray(loc);gl.vertexAttribPointer(loc,2,gl.FLOAT,false,0,0);

function tex(unit,filt,mip){const t=gl.createTexture();gl.activeTexture(gl.TEXTURE0+unit);
 gl.bindTexture(gl.TEXTURE_2D,t);
 gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_S,gl.CLAMP_TO_EDGE);
 gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_T,gl.CLAMP_TO_EDGE);
 gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,mip?gl.LINEAR_MIPMAP_LINEAR:filt);
 gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MAG_FILTER,filt);return t;}
function upload(unit,src,mip){gl.activeTexture(gl.TEXTURE0+unit);
 gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,gl.RGBA,gl.UNSIGNED_BYTE,src);
 if(mip)gl.generateMipmap(gl.TEXTURE_2D);}
const UNITS=[["uPlate",0,gl.LINEAR,false],["uWarpA",1,gl.NEAREST,false],
  ["uWarpB",2,gl.NEAREST,false],["uFrame",3,gl.NEAREST,false],
  ["uFrameMip",4,gl.LINEAR,true],
  ["uDMa",5,gl.NEAREST,false],["uDMb",6,gl.NEAREST,false],
  ["uDCv",7,gl.NEAREST,false],["uDW",8,gl.LINEAR,false],
  ["uGMa",9,gl.NEAREST,false],["uGMb",10,gl.NEAREST,false],
  ["uGCv",11,gl.NEAREST,false],["uGW",12,gl.LINEAR,false],
  ["uLed",13,gl.LINEAR,false]];
for(const [n,u,f,m] of UNITS){tex(u,f,m);gl.uniform1i(gl.getUniformLocation(prog,n),u);}

// ---- test patterns (800x600, CGA palette) ----
const CGA=["#000","#00a","#0a0","#0aa","#a00","#a0a","#a50","#aaa",
           "#555","#55f","#5f5","#5ff","#f55","#f5f","#ff5","#fff"];
const fc=document.createElement("canvas");fc.width=800;fc.height=600;
const fx=fc.getContext("2d");
const sq2img=new Image();
function drawPattern(t){
  fx.fillStyle="#000";fx.fillRect(0,0,800,600);
  if(t==="sq2"&&sq2img.complete){fx.drawImage(sq2img,0,0,800,600);}
  else if(t==="hatch"){fx.strokeStyle="#fff";fx.lineWidth=2;fx.beginPath();
    for(let x=0;x<=800;x+=50){fx.moveTo(x+0.5,0);fx.lineTo(x+0.5,600);}
    for(let y=0;y<=600;y+=50){fx.moveTo(0,y+0.5);fx.lineTo(800,y+0.5);}fx.stroke();}
  else if(t==="dots"){fx.fillStyle="#fff";
    for(let y=25;y<600;y+=50)for(let x=25;x<800;x+=50)fx.fillRect(x-2,y-2,5,5);}
  else if(t==="bars"){const w=800/8;
    for(let i=0;i<8;i++){fx.fillStyle=CGA[i];fx.fillRect(i*w,0,w,300);}
    for(let i=0;i<8;i++){fx.fillStyle=CGA[8+i];fx.fillRect(i*w,300,w,300);}}
  else if(t==="steps"){const w=800/8;
    for(let i=0;i<8;i++){const g=Math.round(i*255/7);
      fx.fillStyle=`rgb(${g},${g},${g})`;fx.fillRect(i*w,0,w,600);}}
  else if(t==="white"){fx.fillStyle="#fff";fx.fillRect(0,0,800,600);}
  upload(3,fc,false);upload(4,fc,true);draw();
}
const PHOS={color:[0,0,0,0],green:[0.22,1,0.4,1],amber:[1,0.64,0.12,1],white:[1,1,1,1]};
function draw(){
  gl.uniform4f(gl.getUniformLocation(prog,"uGeo"),P.hsize,P.vsize,P.hpos,P.vpos);
  gl.uniform4f(gl.getUniformLocation(prog,"uVid"),P.bright,P.contrast,P.gain,P.plate);
  gl.uniform4f(gl.getUniformLocation(prog,"uWin"),WIN.u0,WIN.u1,WIN.v0,WIN.v1);
  gl.uniform1f(gl.getUniformLocation(prog,"uGlowGain"),P.glow);
  gl.uniform1f(gl.getUniformLocation(prog,"uPower"),P.power==="on"?1.0:0.0);
  const ph=PHOS[P.phosphor];
  gl.uniform4f(gl.getUniformLocation(prog,"uPhos"),ph[0],ph[1],ph[2],ph[3]);
  gl.viewport(0,0,1600,1200);
  gl.drawArrays(gl.TRIANGLES,0,3);
  exportJSON();
}
function exportJSON(){
  $("json").textContent=JSON.stringify({
    power:P.power,
    geometry:{h_size:P.hsize,v_size:P.vsize,h_center:P.hpos,v_center:P.vpos},
    video:{brightness:P.bright,contrast:P.contrast,screen_gain:P.gain},
    room:{glow_gain:P.glow,plate_exposure:P.plate},
    phosphor:P.phosphor,
    visible_uv_window:WIN,
    glow_model:"moment transport: per-lobe (diffuse/glossy) Cycles-baked weight + mean + covariance, 5-tap anisotropic gather",
    note:"v in window is Blender convention (0=bottom); flip for scanline order"
  },null,1);
}
// ---- wiring ----
const SL=["hsize","vsize","hpos","vpos","bright","contrast","gain","glow","plate"];
const FMT={hpos:v=>(v>=0?"+":"")+v.toFixed(3),vpos:v=>(v>=0?"+":"")+v.toFixed(3),
           hsize:v=>v.toFixed(3),vsize:v=>v.toFixed(3),bright:v=>v.toFixed(3)};
function show(id){const v=P[id];
  document.querySelector(`output[for=${id}]`).textContent=(FMT[id]||(x=>x.toFixed(2)))(v);}
for(const id of SL){
  $(id).addEventListener("input",e=>{P[id]=parseFloat(e.target.value);show(id);draw();});
}
function setSliders(){for(const id of SL){$(id).value=P[id];show(id);}}
document.querySelectorAll("[data-reset]").forEach(b=>b.addEventListener("click",()=>{
  const g={geo:["hsize","vsize","hpos","vpos"],vid:["bright","contrast","gain"],
           room:["glow","plate"]}[b.dataset.reset];
  for(const id of g)P[id]=DEF[id];setSliders();draw();}));
$("factory").addEventListener("click",()=>{Object.assign(P,DEF);setSliders();
  setChips("phos","data-p",P.phosphor);setChips("pat","data-t",P.pattern);
  setChips("pwr","data-w",P.power);
  drawPattern(P.pattern);});
$("pwr").addEventListener("click",e=>{const b=e.target.closest("button");if(!b)return;
  P.power=b.dataset.w;setChips("pwr","data-w",P.power);draw();});
function setChips(group,attr,val){
  document.querySelectorAll(`#${group} button`).forEach(b=>
    b.setAttribute("aria-pressed",String(b.getAttribute(attr)===val)));}
$("phos").addEventListener("click",e=>{const b=e.target.closest("button");if(!b)return;
  P.phosphor=b.dataset.p;setChips("phos","data-p",P.phosphor);draw();});
$("pat").addEventListener("click",e=>{const b=e.target.closest("button");if(!b)return;
  P.pattern=b.dataset.t;setChips("pat","data-t",P.pattern);drawPattern(P.pattern);});
$("copy").addEventListener("click",()=>{
  navigator.clipboard.writeText($("json").textContent).then(()=>{
    $("copy").textContent="COPIED";setTimeout(()=>$("copy").textContent="COPY JSON",900);});});
// ---- asset loading ----
let pending=12;
function loaded(){if(--pending===0){drawPattern(P.pattern);}}
function img(unit,src,mip){const i=new Image();i.onload=()=>{upload(unit,i,mip);loaded()};i.src=src;return i;}
img(0,"@@PLATE@@",false);
img(13,"@@LED@@",false);
img(1,"@@WARP_A@@",false);
img(2,"@@WARP_B@@",false);
img(5,"@@D_MA@@",false);
img(6,"@@D_MB@@",false);
img(7,"@@D_CV@@",false);
img(8,"@@D_W@@",false);
img(9,"@@G_MA@@",false);
img(10,"@@G_MB@@",false);
img(11,"@@G_CV@@",false);
img(12,"@@G_W@@",false);
sq2img.src="@@SQ2@@";
sq2img.onload=()=>{if(pending===0)drawPattern(P.pattern);};
exportJSON();
</script>
"""

for k, v in ASSETS.items():
    HTML = HTML.replace(k, v)
out = os.path.join(D, "tune_monitor.html")
with open(out, "w", encoding="utf-8") as f:
    f.write(HTML)
print(out, os.path.getsize(out) // 1024, "KB")
