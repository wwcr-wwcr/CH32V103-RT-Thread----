'use strict';
const assert = require('node:assert/strict');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { chromium } = require('playwright');

async function run() {
  const browser = await chromium.launch({channel:'msedge'});
  try {
    const page = await browser.newPage({viewport:{width:1500,height:900}});
    const errors = [], dialogs = [];
    page.on('pageerror', err => errors.push(err.message));
    page.on('dialog', async dialog => { dialogs.push(dialog.message()); await dialog.accept(); });
    const target = process.argv[2] || path.join(__dirname,'../lvgl_designer.html');
    await page.goto(pathToFileURL(path.resolve(target)).href);
    await page.locator('#btnNew').click();
    assert.equal(await page.locator('[data-widget="chart"] span').innerText(),'已启用');
    assert.equal(await page.locator('[data-widget="switch"] span').innerText(),'需启用');
    const palette = page.locator('#paletteFilter');
    await palette.fill('ui_label_at');
    assert.equal(await page.locator('.pal .it:visible').count(),1);
    await page.locator('#zoom').selectOption('0.75');
    await page.locator('.pal .it:visible').dragTo(page.locator('#canvas'),{targetPosition:{x:60,y:84}});
    await page.waitForFunction(() => scr().widgets.length===1);
    const position=await page.evaluate(() => [wid(0).x,wid(0).y]);
    assert.ok(Math.abs(position[0]-80)<2 && Math.abs(position[1]-112)<2,position);
    const picked=(name,text) => ({name,mimeType:'text/plain',buffer:Buffer.from(text)});
    await page.locator('#fileC').setInputFiles([
      picked('task_light.c','#include "sensor.h"\nstatic uint32_t lux;\nstatic char name[12];\nvoid read(void){uint32_t temporary;}\n'),
      picked('sensor.h','#ifndef SENSOR_H\n#define SENSOR_H\nextern double daylight;\nvoid set(uint32_t parameter);\n#endif\n'),
    ]);
    await page.waitForFunction(() => VARPOOL.length===3);
    assert.deepEqual(await page.evaluate(() => VARPOOL.map(v => [v.name,v.type])),
      [['lux','uint32_t'],['name','const char *'],['daylight','double']]);
    assert.deepEqual(await page.locator('#taskFunctions li').allTextContents(),['void read(void)','void set(uint32_t parameter)']);
    assert.deepEqual(await page.evaluate(()=>inspectTaskC('static unsigned short channel;\nrt_int32_t balance;').variables),
      [{name:'channel',type:'uint16_t'},{name:'balance',type:'int32_t'}]);
    await page.locator('#fileC').setInputFiles(picked('board.h','extern "C" {\nextern uint16_t battery;\n}\n'));
    await page.waitForFunction(()=>VARPOOL.some(v=>v.name==='battery'));
    assert.equal(await page.evaluate(()=>TASKNAME),'task_light.c');
    assert.deepEqual(await page.evaluate(()=>TASKHEADERS.map(h=>h.name)),['sensor.h','board.h']);
    await page.locator('#f_bind').fill('daylight');
    await page.locator('#f_bind').press('Tab');
    assert.equal(await page.locator('#f_bindType').inputValue(),'double');
    const output=await page.evaluate(() => genCode());
    assert.ok(output.header.includes('(double daylight);'));
    assert.ok(output.source.includes('(double)(daylight)'));
    const data=await page.evaluate(() => ({version:1,name:'legacy',landscape:true,screens:S.screens}));
    const invalidDialog=page.waitForEvent('dialog');
    await page.locator('#fileIn').setInputFiles(picked('invalid.json','{"screens":[]}'));
    await invalidDialog;
    await page.waitForFunction(() => TASKNAME==='task_light.c');
    assert.equal(dialogs.length,2);
    assert.equal(await page.locator('#f_bind').inputValue(),'daylight');
    await page.locator('#fileIn').setInputFiles(picked('legacy.json',JSON.stringify(data)));
    await page.waitForFunction(() => S.name==='legacy');
    assert.deepEqual(await page.evaluate(() => [TASKNAME,VARPOOL.length,TASKHEADERS.length,history.length]),['',0,0,0]);
    assert.equal(await page.locator('#tabTask').isDisabled(),true);
    await page.locator('#btnTaskGenerator').click();
    await page.locator('#dtg-name').fill('light');
    await page.getByText('User/task_light/task_light_ui.c',{exact:false}).waitFor();
    // Edge exposes origin-private directory handles on loopback, independent of the OS picker.
    await page.route('http://localhost:9724/**',route=>{
      const isModule=new URL(route.request().url()).pathname==='/designer-taskgen.js';
      return route.fulfill({path:isModule?path.join(path.dirname(target),'designer-taskgen.js'):target,
        contentType:isModule?'text/javascript':'text/html; charset=utf-8'});
    });
    await page.goto('http://localhost:9724/lvgl_designer.html');
    await page.locator('#btnTaskGenerator').click();
    await page.locator('#dtg-name').fill('light');
    await page.evaluate(async () => {
      const root=await navigator.storage.getDirectory();
      const user=await root.getDirectoryHandle('User',{create:true});
      const menu=await user.getDirectoryHandle('task_menu',{create:true});
      let lvgl=root;
      for(const dir of ['LVGL','GUI','lvgl']) lvgl=await lvgl.getDirectoryHandle(dir,{create:true});
      for(const [dir,name,data] of [[user,'tasks.h','#include "router.h"\n'],
          [lvgl,'lv_conf.h','#define LV_USE_SWITCH 1\n#define LV_USE_BTNMATRIX 0\n'],
          [menu,'menu.c','static const task_t *const g_app_list[] = {\n};\n']]){
        const handle=await dir.getFileHandle(name,{create:true});
        const writer=await handle.createWritable();
        await writer.write(data); await writer.close();
      }
      window.showDirectoryPicker=async()=>root;
    });
    await page.locator('#dtg-project').click();
    await page.locator('#dtg-register').check();
    await page.getByText('User/task_menu/menu.c',{exact:false}).waitFor();
    await page.locator('#dtg-write').click();
    await page.waitForFunction(()=>document.querySelector('#dtg-status').textContent.includes('已写入'));
    const outputFiles=await page.evaluate(async()=>{
      const user=await (await navigator.storage.getDirectory()).getDirectoryHandle('User');
      const task=await user.getDirectoryHandle('task_light'), menu=await user.getDirectoryHandle('task_menu');
      const files=[];
      for await(const name of task.keys()) files.push(name);
      const menuText=await (await (await menu.getFileHandle('menu.c')).getFile()).text();
      return {files:files.sort(),menuText};
    });
    assert.deepEqual(outputFiles.files,['task_light.c','task_light_ui.c','task_light_ui.h']);
    assert.ok(outputFiles.menuText.includes('extern const task_t task_light;'));
    assert.ok(outputFiles.menuText.includes('&task_light,'));
    await page.locator('#dtg-write').click();
    await page.waitForFunction(()=>document.querySelector('#dtg-status').textContent.includes('文件内容一致'));
    await page.locator('#dtg-close').click();
    assert.equal(await page.locator('[data-widget="switch"] span').innerText(),'已启用');
    await page.locator('#paletteFilter').fill('ui_btnmatrix_new');
    await page.locator('.pal .it:visible').click();
    await page.locator('#f_options').fill('1,2\n3,"4"');
    await page.locator('#f_options').press('Tab');
    assert.deepEqual(await page.locator('.matrix-row span').allTextContents(),['1','2','3','"4"']);
    assert.ok((await page.evaluate(()=>genCode().source)).includes('{"1", "2", "\\n", "3", "\\"4\\"", ""}'));
    await page.locator('#btnTaskGenerator').click();
    await page.waitForFunction(()=>document.querySelector('#dtg-status').textContent.includes('LV_USE_BTNMATRIX'));
    assert.deepEqual(errors,[]);
    console.log('Palette drop, C/H binding, legacy JSON, three-file project write, menu and repeat write passed.');
  } finally { await browser.close(); }
}
run().catch(err => {console.error(err);process.exitCode=1;});
