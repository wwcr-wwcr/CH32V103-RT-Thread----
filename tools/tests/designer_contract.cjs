/* Run with Playwright on NODE_PATH and Edge installed:
 * node tools/tests/designer_contract.cjs [--baseline old.html] [--gcc riscv-none-embed-gcc]
 * --editor-only skips the integrated ZIP tests. --zip-dir retains both test ZIPs.
 * Builds use the project's MRS include paths and defines.
 */
'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { execFileSync } = require('node:child_process');
const { chromium } = require('playwright');

const options = process.argv.slice(2);
const option = name => options.includes(name) ? options[options.indexOf(name) + 1] : undefined;
const root = path.resolve(__dirname, '../..');
const target = path.resolve(option('--html') || path.join(root, 'tools/lvgl_designer.html'));
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'designer-contract-'));
const prefix = 'task_contract_ui';
const widget = (type, name, properties = {}) => ({
  type, name, x: 12, y: 18, w: 92, h: 30, parent: -1, ...properties,
});
const screen = (name, ...widgets) => ({ name, widgets });
const fixtures = [
  { screens: [screen('Empty')], params: 'void', args: '' },
  { screens: [screen('Light',
    widget('label','level',{text:'Lux',color:'TEXT',bind:'lux',bindType:'uint32_t'}),
    widget('btn','sample',{text:'Read',code:17}),
    widget('label','state',{text:'ON',bind:'state',bindType:'const char *'}),
    widget('label','repeat',{text:'Again',bind:'lux',bindType:'uint32_t'}))],
    params: 'uint32_t lux, const char * state', args: 'lux, state' },
  { screens: [
    screen('Light',
      widget('bar','vertical',{barDir:'v',min:0,max:300,val:10,bind:'lux',bindType:'int32_t'}),
      widget('led','power',{bind:'lux',bindType:'int32_t'}),
      widget('arc','scale',{max:300,bind:'lux',bindType:'int32_t'})),
    screen('Charts',
      widget('chart','scope',{dataSrc:'demo',chartMode:'scope',points:32,max:4095}),
      widget('chart','spectrum',{dataSrc:'demo',chartMode:'spectrum',points:24,max:100}),
      widget('chart','external',{dataSrc:'ext',points:16,max:100,bind:'lux',bindType:'int32_t'}))],
    params:'uint8_t screen, int32_t lux', args:'screen, lux' },
];

async function openDesign(page, index, name = prefix) {
  await page.locator('#fileIn').setInputFiles({name: 'old-design.json', mimeType: 'application/json',
    buffer: Buffer.from(JSON.stringify({version:1,name,landscape:true,screens:fixtures[index].screens}))});
  await page.waitForFunction(({name,index}) => document.querySelector('#projName').value === name
    && S.screens[0].name === ['Empty','Light','Light'][index]
    && S.screens.length === (index === 2 ? 2 : 1), {name,index});
}

async function snapshot(browser, file) {
  const page = await browser.newPage();
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  await page.goto(pathToFileURL(file).href);
  const cases = [];
  for (const [index, fixture] of fixtures.entries()) {
    await openDesign(page, index);
    await page.locator('#genTask').check();
    await page.locator('#btnCode').click();
    const out = await page.evaluate(() => genCode());
    assert.equal(await page.locator('#codeOut').inputValue(), out.combined);
    assert.equal(out.prefix, prefix);
    assert.equal(out.renderCall, fixture.args);
    assert.ok(out.header.includes('void '+prefix+'_render('+fixture.params+');'));
    assert.ok(out.header.includes('void '+prefix+'_destroy(void);'));
    assert.ok(out.combined.includes(out.source.trimEnd()));
    assert.ok(out.combined.includes(out.header.trimEnd()));
    assert.ok(out.combined.includes(out.task.trimEnd()));
    assert.ok(out.task.includes('const task_t task_'+prefix+' ='));
    await page.locator('#btnCloseCode').click();
    cases.push(out);
  }
  await openDesign(page, 1);
  const imported = '#include "ui/old_ui.h"\nstatic uint32_t lux = 0;\nstatic const char *state;\n'
    +'static void redraw(void) { old_ui_render(); }\nconst task_t t = { .ui_destroy = old_ui_destroy };\n';
  await page.locator('#fileC').setInputFiles({name:'task_light.c',mimeType:'text/plain',buffer:Buffer.from(imported)});
  await page.waitForFunction(() => document.querySelector('#tabTask').disabled === false);
  await page.locator('#btnCode').click();
  await page.locator('#tabTask').click();
  const patched = await page.locator('#codeOut').inputValue();
  assert.ok(patched.includes(prefix+'_render(lux, state);'));
  assert.ok(patched.includes('.ui_destroy = '+prefix+'_destroy'));
  assert.ok(patched.includes(prefix+'.h"'));
  assert.deepEqual(errors, []);
  await page.close();
  return cases;
}

function declarations(code) {
  return code.split('\n').filter(line => /^(?:void\s|int16_t\s*\*|#define\s+\w+_(?:ID_|SCREEN_CNT))/.test(line)).sort();
}

function checkGeneration(cases, baseline) {
  if (baseline) {
    // A design without bindings or charts still exports byte-for-byte identical code.
    assert.deepEqual(cases[0], baseline[0]);
    for (const [i,out] of cases.entries()) {
      const previous = declarations(baseline[i].header);
      const current = declarations(out.header);
      if (i === 2) previous.push('void '+prefix+'_vertical_set(int32_t v);');
      assert.deepEqual(current, previous.sort());
      assert.equal(out.renderCall, baseline[i].renderCall);
    }
    assert.equal(cases[1].source, baseline[1].source);
    assert.equal(cases[1].header, baseline[1].header);
  }
  assert.match(cases[1].header, /#define TASK_CONTRACT_UI_ID_SAMPLE\s+17/);
  assert.match(cases[1].task, /task_contract_ui_render\(\(uint32_t\)s_lux, s_state\);/);
  const charts = cases[2];
  for (const method of ['vertical_set','scope_push','scope_set','scope_frame','spectrum_bins','external_data','external_flush'])
    assert.ok(charts.header.includes(prefix+'_'+method+'('));
  assert.match(charts.source, /task_contract_ui_vertical_set\(\(int32_t\)\(lux\)\);/);
  const declarationsInSource = [...charts.source.matchAll(/^static lv_obj_t \*(w\w*) = NULL;/gm)].map(m => m[1]);
  assert.equal(new Set(declarationsInSource).size, declarationsInSource.length);
  for (const [,variable] of charts.source.matchAll(/= lv_chart_get_series_next\((\w+), NULL\);/g)) {
    const creation = charts.source.indexOf(variable+' = ui_chart_new(');
    assert.ok(creation >= 0 && creation < charts.source.indexOf('= lv_chart_get_series_next('+variable+', NULL);'));
  }
  assert.doesNotMatch(charts.source, /task_contract_ui_external_push\(/);
}

function compile(name, folder, compiler) {
  const projectFile = fs.readdirSync(root).find(name => name.endsWith('.wvproj'));
  const config = JSON.parse(fs.readFileSync(path.join(root,projectFile),'utf8').replace(/^\uFEFF/,''))
    .buildConfig.configurations[0].ccompiler;
  const flags = ['-march=rv32imacxw','-mabi=ilp32','-std=gnu99','-Os',
    ...config.preprocessor.defined_symbols.map(value => '-D'+value),
    ...config.includes.include_paths.map(value => '-I'+value.replace('${project}/',''))];
  const objects = [];
  for (const source of [name+'.c',name+'_ui.c']) {
    const obj = path.join(folder, source+'.o');
    execFileSync(compiler,[...flags,'-Werror=implicit-function-declaration','-c',path.join(folder,source),'-o',obj],
      {cwd:root,stdio:'inherit'});
    objects.push(obj);
  }
  const merged = path.join(folder,'check.o');
  execFileSync(compiler,['-march=rv32imacxw','-mabi=ilp32','-nostdlib','-r',...objects,'-o',merged],{cwd:root,stdio:'inherit'});
  const nm = compiler.replace(/gcc(?:\.exe)?$/i, match => match.toLowerCase().endsWith('.exe') ? 'nm.exe' : 'nm');
  const symbols = execFileSync(nm,[merged],{encoding:'utf8'});
  for (const symbol of [name,name+'_ui_render',name+'_ui_destroy'])
    assert.match(symbols,new RegExp(' [A-TV-Z] '+symbol+'\\r?$','m'));
  assert.doesNotMatch(symbols, new RegExp(' U '+name+'(?:_ui)?_'));
}

async function checkPackage(browser, compiler) {
  const page = await browser.newPage({acceptDownloads:true});
  await page.goto(pathToFileURL(target).href);
  // Reload the editor for each export so that no package state carries across.
  for (const index of [1,2]) {
    await openDesign(page, index);
    await page.locator('#btnTaskGenerator').click();
    await page.locator('#dtg-name').fill('contract');
    const downloadPromise = page.waitForEvent('download');
    await page.locator('#dtg-zip').click();
    const download = await downloadPromise;
    const zip = path.join(temporary, 'case-'+index+'.zip');
    await download.saveAs(zip);
    const zipDir = option('--zip-dir');
    if (zipDir) {
      fs.mkdirSync(zipDir,{recursive:true});
      fs.copyFileSync(zip,path.join(zipDir,path.basename(zip)),fs.constants.COPYFILE_EXCL);
    }
    const folder = path.join(temporary, 'case-'+index);
    const unzip = [
      'import pathlib, sys, zipfile',
      'out = pathlib.Path(sys.argv[2]); out.mkdir()',
      'expected = {"User/task_contract/"+n for n in ("task_contract.c","task_contract_ui.c","task_contract_ui.h")}',
      'with zipfile.ZipFile(sys.argv[1]) as z:',
      ' files = [i.filename for i in z.infolist() if not i.is_dir()]',
      ' assert sorted(files) == sorted(expected), files',
      ' for name in files:',
      '  b=z.read(name); text=b.decode("gbk"); assert "\\r\\n" in text and "\\n" not in text.replace("\\r\\n",""), name',
      '  (out/pathlib.PurePosixPath(name).name).write_bytes(b)',
    ].join('\n');
    execFileSync(process.env.PYTHON || 'python',['-c',unzip,zip,folder],{stdio:'inherit'});
    const task = new TextDecoder('gbk').decode(fs.readFileSync(path.join(folder,'task_contract.c')));
    const header = new TextDecoder('gbk').decode(fs.readFileSync(path.join(folder,prefix+'.h')));
    assert.ok(task.includes('#include "'+prefix+'.h"'));
    assert.ok(task.includes('const task_t task_contract'));
    assert.ok(task.includes(prefix+'_render('));
    assert.ok(header.includes(prefix+'_render('+fixtures[index].params+');'));
    if (compiler) compile('task_contract',folder,compiler);
    await page.reload();
  }
  await page.close();
}

(async () => {
  const browser = await chromium.launch({ channel: process.env.BROWSER_CHANNEL || 'msedge', headless:true });
  try {
    const before = option('--baseline');
    const baseline = before && await snapshot(browser,path.resolve(before));
    checkGeneration(await snapshot(browser,target), baseline);
    if (!options.includes('--editor-only')) await checkPackage(browser, option('--gcc'));
    console.log('Designer export contract passed'+(option('--gcc') ? ' with RISC-V compilation and relocatable linking.' : '.'));
  } finally {
    await browser.close();
    assert.equal(path.dirname(path.resolve(temporary)),path.resolve(os.tmpdir()));
    fs.rmSync(temporary,{recursive:true,force:true});
  }
})().catch(error=>{console.error(error);process.exitCode=1;});
