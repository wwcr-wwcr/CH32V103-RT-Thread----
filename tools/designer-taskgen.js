/* Offline integration for the task_t project and tools/new_task.py. */
(() => {
  'use strict';

  const bundledTemplate = "/******************************************************************************\n * 文件名   : task_name.c\n * 说明     : 任务骨架 —— 由 tools/new_task.py 生成，把 task_name 换成真名\n *\n * 函数命名约定（都是 static，各文件互不影响）：\n *     task_open / task_close / task_post / task_render / task_ui_destroy\n *     task_thread / task_tick / task_key / task_set\n ******************************************************************************/\n\n#include \"router.h\"\n#include \"key.h\"\n#include \"task_name_ui.h\"\n#include \"rtthread.h\"\n\nstatic rt_sem_t    s_exit = RT_NULL;    /* 退出握手：线程 exit，close 等它 */\nstatic rt_mq_t     s_q    = RT_NULL;    /* 自己的队列 */\nstatic void task_thread(void *arg);\n\n\n//********************在这里写常用变量********************//\n\n\n\n\nstatic void task_open(void)\n{\n    rt_thread_t tid;\n\n    if(s_q != RT_NULL)\n    {\n        return;                                 /* 已经在跑 */\n    }\n\n    s_exit = rt_sem_create(\"sem\", 0, RT_IPC_FLAG_FIFO);\n    s_q    = rt_mq_create(\"mq\", sizeof(message), 4, RT_IPC_FLAG_FIFO);\n    tid    = rt_thread_create(\"thr\", task_thread, RT_NULL, 1024, 12, 10);\n\n    if((s_exit == RT_NULL) || (s_q == RT_NULL) || (tid == RT_NULL))\n    {\n        rt_kprintf(\"task_name: open failed\\r\\n\");\n\n        if(tid    != RT_NULL) { rt_thread_delete(tid); }\n        if(s_q    != RT_NULL) { rt_mq_delete(s_q);       s_q    = RT_NULL; }\n        if(s_exit != RT_NULL) { rt_sem_delete(s_exit);   s_exit = RT_NULL; }\n        return;\n    }\n//********************在这里写进入逻辑********************//\n\n\n\n\n    rt_thread_startup(tid);\n}\n\n\nstatic void task_tick(void)\n{\n//********************在这里写循环逻辑********************//\n\n}\n\n\nstatic void task_key(key_id_t key)\n{\n//********************在这里写按键逻辑********************//\n    switch(key)\n    {\n        case KEY_ID_1:\n            break;\n\n        case KEY_ID_2:\n            break;\n\n        case KEY_ID_3:\n            break;\n\n        case KEY_ID_4:\n            break;\n\n        default:                                /* 长按 / 双击暂不处理 */\n            return;\n    }\n}\n\n\nstatic void task_set(uint8_t id, int32_t value)\n{\n//********************在这里写控件逻辑********************//\n    (void)value;\n\n    switch(id)\n    {\n        case 1:                                 /* id = 1 的控件 */\n            break;\n\n        default:\n            return;\n    }\n}\n\n\nstatic void task_close(void)\n{\n    message bye = {2, 0, 0};                    /* 2 = QUIT，该退出了 */\n    message drop;\n    uint8_t retry;\n\n    if(s_q == RT_NULL)\n    {\n        return;                                 /* 没开，什么都不用做 */\n    }\n//********************在这里写退出逻辑********************//\n\n\n\n\n    /* 队列可能刚好满：腾一个位置再投，最多试 20 次 */\n    for(retry = 0; retry < 20; retry++)\n    {\n        if(rt_mq_send(s_q, &bye, sizeof(bye)) == RT_EOK)\n        {\n            break;\n        }\n        (void)rt_mq_recv(s_q, &drop, sizeof(drop), 0);\n    }\n\n    if(retry >= 20)\n    {\n        rt_kprintf(\"task_name: quit msg failed\\r\\n\");\n        return;                                 /* 投不进去就别动，线程还在用这个队列 */\n    }\n\n    if(rt_sem_take(s_exit, rt_tick_from_millisecond(3000)) != RT_EOK)\n    {\n        rt_kprintf(\"task_name: exit timeout\\r\\n\");\n        return;                                 /* 线程还没走，队列不能删 */\n    }\n\n    /* 线程已经自己退出（entry return），这里只回收资源 */\n    rt_mq_delete(s_q);      s_q    = RT_NULL;\n    rt_sem_delete(s_exit);  s_exit = RT_NULL;\n}\n\n\nstatic void task_render(void)\n{\n    /* 贴完设计器生成的 UI 后，把下面这行的注释去掉，按你的数据传参 */\n    /* task_name_ui_render(); */\n}\n\n\nstatic void task_ui_destroy(void)\n{\n    /* 贴完设计器生成的 UI 后，把下面这行的注释去掉 */\n    /* task_name_ui_destroy(); */\n}\n\n\nstatic void task_thread(void *arg)\n{\n    message msg;\n\n    (void)arg;\n\n    for(;;)\n    {\n        /* 10ms 一拍：有事件就处理，没事件也照样往下跑周期活 */\n        if(rt_mq_recv(s_q, &msg, sizeof(msg), rt_tick_from_millisecond(10)) == RT_EOK)\n        {\n            if(msg.type == QUIT)                /* 管理端让退出 */\n            {\n                break;\n            }\n\n            if(msg.type == KEY)                 /* 实体按键 */\n            {\n                task_key(msg.code);\n            }\n            else                                /* 触摸控件 */\n            {\n                task_set(msg.code, msg.value);\n            }\n        }\n\n        task_tick();\n    }\n\n    rt_sem_release(s_exit);                     /* 我走了，close 那边可以往下走 */\n}\n\n\nstatic void task_post(const message *msg)\n{\n    if(s_q != RT_NULL)\n    {\n        (void)rt_mq_send(s_q, (void *)msg, sizeof(*msg));   /* 队列只读，转掉 const */\n    }\n}\n\n\n/**************************************************************************************************\n *                                        任务描述\n **************************************************************************************************/\nconst task_t task_name =\n{\n    .name       = \"NAME\",           /* 线程名，<= 7 字符 */\n    .keep       = 0,                /* 1 = 前台常驻 */\n\n    .open       = task_open,        /* 进入 task */\n    .close      = task_close,       /* 退出 task */\n    .post       = task_post,        /* 事件收取 */\n    .render     = task_render,      /* 接管屏幕刷新 */\n    .ui_destroy = task_ui_destroy,  /* lvgl 卸载 */\n};\n";
  const menuPath = 'User/task_menu/menu.c';

  function normalize(name) {
    let value = String(name || '').trim().toLowerCase().replace(/[- ]/g, '_');
    if (!value) return null;
    if (!value.startsWith('task_')) value = 'task_' + value;
    return /^task_[a-z0-9_]+$/.test(value) ? value : null;
  }

  function decode(bytes) {
    const bom = bytes[0] === 0xef && bytes[1] === 0xbb && bytes[2] === 0xbf;
    try {
      return {text: new TextDecoder('utf-8', {fatal:true}).decode(bytes), encoding:'utf-8', bom};
    } catch {
      return {text:new TextDecoder('gbk', {fatal:true}).decode(bytes), encoding:'gbk', bom:false};
    }
  }

  let gbkTable;
  function encodeGBK(text) {
    if (!gbkTable) {
      const table = new Map();
      const decoder = new TextDecoder('gbk', {fatal:true});
      const pair = new Uint8Array(2);
      for (let lead = 0x81; lead <= 0xfe; lead++) {
        for (let trail = 0x40; trail <= 0xfe; trail++) {
          if (trail === 0x7f) continue;
          pair[0] = lead;
          pair[1] = trail;
          try {
            const ch = decoder.decode(pair);
            if (ch.length === 1 && !table.has(ch)) table.set(ch, lead * 256 + trail);
          } catch { /* Unassigned code point. */ }
        }
      }
      gbkTable = table;
    }
    const result = [];
    for (const ch of text) {
      if (ch.codePointAt(0) < 128) result.push(ch.charCodeAt(0));
      else {
        const code = gbkTable.get(ch);
        if (code === undefined) throw new Error('GBK 无法编码字符「' + ch + '」，请更换文字后生成。');
        result.push(code >> 8, code & 255);
      }
    }
    return new Uint8Array(result);
  }

  function encode(text, original) {
    const eol = original && /\r?\n/.exec(original.text);
    const withEol = text.replace(/\r\n|\r|\n/g, eol ? eol[0] : '\r\n');
    if (!original || original.encoding === 'gbk') return encodeGBK(withEol);
    return new TextEncoder().encode((original.bom ? '\ufeff' : '') + withEol);
  }

  function same(a, b) {
    if (a === null || b === null) return a === b;
    return a.length === b.length && a.every((x, i) => x === b[i]);
  }

  /* Mask comments and quoted literals without moving C token offsets. */
  function maskC(source) {
    return source.replace(/\/\*[\s\S]*?\*\/|\/\/[^\r\n]*|"(?:\\[\s\S]|[^"\\])*"|'(?:\\[\s\S]|[^'\\])*'/g,
                          token => token.replace(/[^\r\n]/g, ' '));
  }

  function closing(text, at, open, close) {
    let depth = 1;
    for (let i = at + 1; i < text.length; i++) {
      if (text[i] === open) depth++;
      if (text[i] === close && --depth === 0) return i;
    }
    throw new Error('C 源码的括号不完整。');
  }

  const cKeywords = new Set(('auto break case char const continue default do double else enum extern float for goto if inline int long register restrict return short signed sizeof static struct switch typedef union unsigned void volatile while _Alignas _Alignof _Atomic _Bool _Complex _Generic _Imaginary _Noreturn _Static_assert _Thread_local').split(' '));
  function parameters(header, prefix) {
    if (!header) return null;
    const match = new RegExp('\\bvoid\\s+' + prefix + '_render\\s*\\(([^()]*)\\)\\s*;').exec(maskC(header));
    if (!match) return null;
    if (/^\s*(?:void)?\s*$/.test(match[1])) return [];
    const names = new Set();
    return match[1].split(',').map(param => {
      const decl = /^\s*((?:const\s+)?(?:char|float|double|u?int(?:8|16|32|64)_t)\s*\*?)\s*([A-Za-z_]\w*)\s*$/.exec(param);
      if (!decl || names.has(decl[2]) || cKeywords.has(decl[2])) throw new Error('render 参数名重复、无效、使用 C 关键字或类型不兼容：' + param);
      names.add(decl[2]);
      return {name:decl[2], type:decl[1].trim()};
    });
  }

  function splitArguments(text) {
    const masked = maskC(text);
    let start = 0, depth = 0;
    const args = [];
    for (let i = 0; i < masked.length; i++) {
      const ch = masked[i];
      if ('([{'.includes(ch)) depth++;
      if (')]}'.includes(ch)) depth--;
      if (ch === ',' && depth === 0) { args.push(text.slice(start,i).trim()); start = i+1; }
    }
    if (text.trim()) args.push(text.slice(start).trim());
    return args;
  }

  function callbackRange(source, name, member) {
    const masked = maskC(source);
    const desc = new RegExp('\\bconst\\s+task_t\\s+' + name + '\\s*=\\s*\\{').exec(masked);
    if (!desc) throw new Error('业务文件中缺少 const task_t ' + name + ' 描述符。');
    const begin = desc.index + desc[0].length - 1;
    const body = masked.slice(begin, closing(masked, begin, '{','}'));
    const assignment = new RegExp('\\.' + member + '\\s*=\\s*(\\w+)').exec(body);
    if (!assignment) throw new Error('任务描述符缺少 .' + member + '。');
    const callback = assignment[1];
    const definition = new RegExp('\\bvoid\\s+' + callback + '\\s*\\(\\s*(?:void)?\\s*\\)\\s*\\{').exec(masked);
    if (!definition) return {callback};
    const start = definition.index + definition[0].length;
    return {callback, start, end:closing(masked,start-1,'{','}')};
  }

  function globalDeclarations(source) {
    const masked = maskC(source);
    let fileScope = '';
    for (let i = 0; i < masked.length; i++) {
      if (masked[i] === '{') {
        i = closing(masked,i,'{','}');
        fileScope += ';\n';
      } else fileScope += masked[i];
    }
    return fileScope;
  }

  function declares(text, name, externalOnly=false) {
    const decls = text.matchAll(/(?:^|(?<=[;\n]))\s*((?:(?:extern|static|volatile|const|signed|unsigned)\s+)*[a-zA-Z_]\w*(?:\s+(?:const|volatile))?\s+\*?[^;{}()#]+);/g);
    for (const m of decls) {
      if (externalOnly && !/\bextern\b/.test(m[1])) continue;
      for (const item of m[1].split(',')) {
        const left = item.split('=')[0].replace(/\[[^\]]*\]/g,'').trim();
        if (new RegExp('\\b' + name + '$').test(left)) return true;
      }
    }
    return false;
  }

  function wireTask(source, name, code, oldHeader, headers, warnings) {
    const prefix = name + '_ui';
    const params = parameters(code.header, prefix);
    if (!params) throw new Error('设计器头文件未提供 ' + prefix + '_render 接口。');
    const previous = parameters(oldHeader, prefix);
    const globals = globalDeclarations(source);
    const missing = new Map();
    const all = maskC(source);
    const includes = new Set();
    const exported = headers.filter(h=>h.name.split('/').pop() !== prefix+'.h')
      .map(h=>({name:h.name, declarations:maskC(h.text)}));
    function externalHeader(param) {
      const owners=exported.filter(h=>declares(h.declarations,param.name,true));
      if (!owners.length) return false;
      const activeIncludes = [...source.matchAll(/^[ \t]*#[ \t]*include[ \t]*[<"]([^>"]+)[>"]/gm)]
        .filter(m=>all.slice(m.index,m.index+m[0].length).includes('#'));
      if (!activeIncludes.some(m=>owners.some(h=>m[1]===h.name || m[1].endsWith('/'+h.name)))) {
        includes.add(owners[0].name);
      }
      return true;
    }
    function valueFor(param) {
      const token = new RegExp('\\b' + param.name + '\\b');
      if (declares(globals,param.name)) return param.name;
      if (externalHeader(param)) return param.name;
      if (token.test(all)) throw new Error('绑定「' + param.name + '」在任务文件中不是文件级变量。请使用 render 回调可访问的文件级读数。');
      missing.set(param.name, param);
      return param.name;
    }
    function argumentsFor(current) {
      const args = splitArguments(current);
      if (args.length && !previous) {
        if (!args.every(a=>/^[A-Za-z_]\w*$/.test(a))) throw new Error('已有 render 调用使用了自定义表达式，请一并选择原 '+prefix+'.h 以匹配参数。');
        return params.map(p=>{
          if (!args.includes(p.name)) return valueFor(p);
          if (!declares(globals,p.name)) externalHeader(p);
          return p.name;
        }).join(', ');
      }
      if (args.length && previous.length !== args.length) throw new Error('已有 render 调用和旧头文件的参数数量不一致。');
      return params.map(p => {
        const i = previous ? previous.findIndex(old => old.name === p.name) : -1;
        if (i < 0 || i >= args.length) return valueFor(p);
        if (args[i] === p.name && !declares(globals,p.name)) externalHeader(p);
        return args[i];
      }).join(', ');
    }
    function hook(member, suffix, argsFn) {
      const target = prefix + '_' + suffix;
      const fn = callbackRange(source,name,member);
      if (fn.start === undefined) {
        if (fn.callback === target && (suffix === 'destroy' || !params.length)) return;
        throw new Error('无法在业务文件中定位 .' + member + ' 的本地回调 ' + fn.callback + '。');
      }
      const body = source.slice(fn.start,fn.end);
      const masked = maskC(body);
      const callPattern = new RegExp('\\b' + target + '\\s*\\(', 'g');
      const replacements = [];
      let match;
      while ((match = callPattern.exec(masked))) {
        const begin = match.index + match[0].length;
        const end = closing(masked,begin-1,'(',')');
        replacements.push({start:begin,end,text:argsFn(body.slice(begin,end))});
        callPattern.lastIndex = end+1;
      }
      if (!replacements.length) {
        const placeholder = new RegExp('/\\*\\s*' + target + '\\s*\\(\\s*\\)\\s*;\\s*\\*/').exec(body);
        const call = target + '(' + argsFn('') + ');';
        if (placeholder && !masked.slice(placeholder.index,placeholder.index+placeholder[0].length).trim()) {
          replacements.push({start:placeholder.index,end:placeholder.index+placeholder[0].length,text:call});
        } else if (!masked.trim()) {
          replacements.push({start:body.length,end:body.length,text:'    '+call+'\n'});
        } else throw new Error('已有业务回调 ' + fn.callback + ' 包含自定义逻辑，未找到可同步的 ' + target + '() 调用。请先把该调用加入回调。');
      }
      for (const p of replacements.reverse()) {
        source = source.slice(0,fn.start+p.start) + p.text + source.slice(fn.start+p.end);
      }
    }
    hook('render','render',argumentsFor);
    hook('ui_destroy','destroy',() => '');
    if (!new RegExp('^\\s*#\\s*include\\s*"'+prefix+'\\.h"', 'm').test(source))
      source = '#include "' + prefix + '.h"\n' + source;
    if (includes.size) {
      source = [...includes].map(h=>'#include "'+h+'"\n').join('') + source;
      warnings.push('已引用 '+[...includes].join('、')+'；请确保编译器的头文件搜索路径包含对应目录。');
    }
    if (missing.size) {
      const lastInclude = [...source.matchAll(/^[ \t]*#[ \t]*include[^\r\n]*\r?$/gm)].pop();
      const at = lastInclude.index + lastInclude[0].trimEnd().length;
      const vars = Array.from(missing.values(), p => {
        const str = p.type.includes('*');
        return 'static ' + (str ? p.type : 'volatile ' + p.type) + ' ' + p.name + ' = ' + (str ? '""' : '0') + ';';
      });
      source = source.slice(0,at) + '\n\n' + vars.join('\n') + source.slice(at);
      warnings.push('已为 ' + [...missing.keys()].join('、') + ' 添加初值。请在任务中更新这些变量。');
    }
    return source;
  }

  function registerMenu(text, name) {
    const masked = maskC(text);
    const list = /\bstatic\s+const\s+task_t\s*\*\s*const\s+g_app_list\s*\[\s*\]\s*=\s*\{/.exec(masked);
    if (!list) throw new Error(menuPath + ' 中未找到 g_app_list[]，请检查所选工程。');
    const start = list.index+list[0].length, end = closing(masked,start-1,'{','}');
    const body = masked.slice(start,end);
    if (/[#]/.test(body) || !/^(?:\s*&\s*[A-Za-z_]\w*\s*,)*\s*(?:&\s*[A-Za-z_]\w*)?\s*$/.test(body))
      throw new Error('菜单任务表含条件编译或自定义项，请手动登记任务。');
    const items = [...body.matchAll(/&\s*([A-Za-z_]\w*)/g)].map(x=>x[1]);
    if (!items.includes(name)) {
      const limit = /\bs_names\s*\[\s*(\d+)\s*\]/.exec(masked);
      if (limit && items.length >= Number(limit[1])) throw new Error('菜单 s_names 的容量为 '+limit[1]+'，当前已满，请先扩展菜单或取消菜单登记。');
      const eol = text.includes('\r\n') ? '\r\n' : '\n';
      let insert = text.lastIndexOf('\n',end-1)+1;
      if (insert < start || text.slice(insert,end).trim()) insert = end;
      const line = (insert === end && text[insert-1] !== '\n' ? eol : '') + '    &' + name + ',' + eol;
      text = text.slice(0,insert) + line + text.slice(insert);
      if (items.length && !body.trimEnd().endsWith(',')) {
        const last = [...body.matchAll(/&\s*[A-Za-z_]\w*/g)].pop();
        const at = start+last.index+last[0].length;
        text = text.slice(0,at) + ',' + text.slice(at);
      }
    }
    if (!new RegExp('\\bextern\\s+const\\s+task_t\\s+' + name + '\\s*;').test(masked.slice(0,list.index))) {
      const eol = text.includes('\r\n') ? '\r\n' : '\n';
      text = text.slice(0,list.index) + 'extern const task_t ' + name + ';' + eol + text.slice(list.index);
    }
    return text;
  }

  async function directory(root, parts, create=false) {
    let dir = root;
    for (const part of parts) dir = await dir.getDirectoryHandle(part,{create});
    return dir;
  }

  async function read(root, path) {
    if (!root) return null;
    const parts = path.split('/');
    const name = parts.pop();
    try {
      const dir = await directory(root,parts);
      const handle = await dir.getFileHandle(name);
      return new Uint8Array(await (await handle.getFile()).arrayBuffer());
    } catch (err) {
      if (err.name === 'NotFoundError') return null;
      throw err;
    }
  }

  function filePlan(path, text, before) {
    const original = before === null ? null : decode(before);
    const bytes = original && original.text === text ? before : encode(text,original);
    return {path, name:path.split('/').pop(), text, bytes, before, exists:before !== null, changed:!same(before,bytes)};
  }

  async function prepare({name, root=null, code, register=false, taskSource=null, taskFilename='', template, headers=[]}) {
    name = normalize(name);
    if (!name) throw new Error('任务名只能使用英文字母、数字和下划线，例如 task_light。');
    if (name === 'task_menu') throw new Error('task_menu 是工程自带的主菜单，请换一个任务名。');
    const prefix = name + '_ui';
    if (!code || code.prefix !== prefix || !code.header || !code.source) throw new Error('当前画布的导出前缀应为 ' + prefix + '。');
    if (register && !root) throw new Error('登记菜单需要先选择工程根目录。');
    if (root && !(await read(root,'User/tasks.h'))) throw new Error('请选择含 User/tasks.h 的工程根目录。');
    const paths = [name+'.c',prefix+'.c',prefix+'.h'].map(f=>'User/'+name+'/'+f);
    const current = await Promise.all(paths.map(path=>read(root,path)));
    const warnings = [];
    if (code.disabledMacros?.length)
      warnings.push('当前设计用到的控件需在 lv_conf.h 启用：' + code.disabledMacros.join('、') + '。');
    let source;
    if (current[0] !== null) {
      source = decode(current[0]).text;
      if (taskSource && source !== taskSource) warnings.push('业务代码取自所选工程的 ' + paths[0] + '。');
    } else if (taskSource) {
      if (taskFilename && taskFilename !== name+'.c') throw new Error('导入的业务文件 ' + taskFilename + ' 和任务名不一致。');
      source = taskSource;
    } else {
      let actual = template;
      if (actual == null && root) {
        const local = await read(root,'tools/templates/task_template.c');
        if (local !== null) actual = decode(local).text;
      }
      source = (actual == null ? bundledTemplate : actual)
        .replace(/task_name/g, name).replace(/"NAME"/g, '"' + name.slice(5,11).toUpperCase() + '"');
    }
    for (const h of headers) {
      if (!h || typeof h.text !== 'string' || !/\.h$/i.test(h.name) || /[\\\x00-\x1f<>"]/.test(h.name) || h.name.split('/').some(p=>!p || p==='..' || p==='.'))
        throw new Error('头文件名称或内容无效。');
    }
    const importedHeader = headers.find(h=>h.name.split('/').pop() === prefix+'.h');
    const oldHeader = current[2] !== null ? decode(current[2]).text : importedHeader?.text;
    source = wireTask(source,name,code,oldHeader,headers,warnings);
    const files = [source, code.source, code.header].map((content,i)=>filePlan(paths[i],content,current[i]));
    let menu = null;
    if (register) {
      const bytes = await read(root,menuPath);
      if (bytes === null) throw new Error('找不到 '+menuPath+'，请检查工程或取消登记。');
      const original = decode(bytes).text;
      menu = filePlan(menuPath,registerMenu(original,name),bytes);
    }
    return {name, root, files, menu, warnings};
  }

  async function writeBytes(dir, name, bytes, create) {
    const handle = await dir.getFileHandle(name,{create});
    const stream = await handle.createWritable();
    try { await stream.write(bytes); await stream.close(); }
    catch(err) { try { await stream.abort(); } catch {} throw err; }
  }

  async function commit(plan, overwrite) {
    const {root, name} = plan;
    if (!root) throw new Error('请先选择工程目录。');
    if (!overwrite && plan.files.some(f=>f.exists && f.changed)) throw new Error('已有文件需要更新。请勾选「更新已有文件」后再写入。');
    const entries = [...plan.files, ...(plan.menu ? [plan.menu] : [])];
    const writes = entries.filter(f=>f.changed);
    async function check(file) {
      if (!same(await read(root,file.path),file.before)) throw new Error(file.path+' 在预览后有变动，请重新生成。');
    }
    await Promise.all(entries.map(check));
    if (!writes.length) return [];
    const user = await directory(root,['User']);
    let madeDir = false;
    let taskDir;
    try { taskDir = await user.getDirectoryHandle(name); }
    catch(e) {
      if (e.name !== 'NotFoundError') throw e;
      taskDir = await user.getDirectoryHandle(name,{create:true});
      madeDir = true;
    }
    const touched = [];
    try {
      for (const file of writes) {
        await check(file);
        const dir = file === plan.menu ? await directory(user,['task_menu']) : taskDir;
        touched.push({file,dir});
        await writeBytes(dir,file.name,file.bytes,true);
      }
    } catch (err) {
      const unrestored = [];
      for (const {dir,file} of touched.reverse()) {
        try {
          const now = await read(root,file.path);
          if (same(now,file.before)) continue;
          if (!same(now,file.bytes) && !(file.before === null && now && now.length===0)) {
            unrestored.push(file.path);
          } else if (file.before === null) await dir.removeEntry(file.name);
          else await writeBytes(dir,file.name,file.before,false);
        } catch { unrestored.push(file.path); }
      }
      if (madeDir) { try { await user.removeEntry(name); } catch {} }
      if (unrestored.length) throw new Error(err.message+'；以下文件无法恢复，请检查：'+unrestored.join('、'));
      throw err;
    }
    return writes.map(f=>f.path);
  }

  async function write(plan,{overwrite=false}={}) {
    if (globalThis.navigator?.locks)
      return navigator.locks.request('lvgl-designer-task-'+plan.root?.name,()=>commit(plan,overwrite));
    return commit(plan,overwrite);
  }

  function zip(files) {
    const parts = [], central = [];
    const stamp = new Date();
    const time = (stamp.getHours()<<11) | (stamp.getMinutes()<<5) | (stamp.getSeconds()>>1);
    const day = ((stamp.getFullYear()-1980)<<9) | ((stamp.getMonth()+1)<<5) | stamp.getDate();
    function record(fields) {
      const buf = new Uint8Array(fields.reduce((n,f)=>n+f[0],0)), view = new DataView(buf.buffer);
      let at=0;
      for (const [width,v] of fields) {
        if (width===2) view.setUint16(at,v,true); else view.setUint32(at,v,true);
        at+=width;
      }
      return buf;
    }
    let offset=0, size=0;
    for (const f of files) {
      let crc = 0xffffffff;
      for (const byte of f.bytes) {
        crc ^= byte;
        for (let b=0;b<8;b++) crc=(crc>>>1)^((crc&1)?0xedb88320:0);
      }
      const fname = new TextEncoder().encode(f.path), len = f.bytes.length;
      const meta = [[2,20],[2,0x800],[2,0],[2,time],[2,day],[4,~crc],[4,len],[4,len],[2,fname.length],[2,0]];
      const head=record([[4,0x04034b50],...meta]);
      const dir=record([[4,0x02014b50],[2,20],...meta,[2,0],[2,0],[2,0],[4,0],[4,offset]]);
      parts.push(head,fname,f.bytes); central.push(dir,fname);
      offset += head.length + fname.length + len; size += dir.length + fname.length;
    }
    const n=files.length;
    return new Blob([...parts,...central,record([[4,0x06054b50],[2,0],[2,0],[2,n],[2,n],[4,size],[4,offset],[2,0]])],{type:'application/zip'});
  }

  let panel, project, busy=false, previewId=0, preview=null;
  const byId = id => document.getElementById('dtg-'+id);
  function canvasCode(name) {
    const input = document.getElementById('projName');
    const prev = input.value, saved = S.name;
    try {
      input.value=name+'_ui';
      const code=genCode();
      code.disabledMacros=disabledWidgetMacros();
      return code;
    }
    finally { input.value=prev; S.name=saved; }
  }
  function inputs(forZip=false) {
    const name = normalize(byId('name').value);
    if (!name) throw new Error('请填写任务名，例如 task_light。');
    const imported = typeof TASKSRC !== 'undefined' && typeof TASKNAME !== 'undefined' && TASKNAME === name+'.c';
    return {name, root:project || null, code:canvasCode(name), register:!forZip && !!project && byId('register').checked,
            taskSource:imported ? TASKSRC : null, taskFilename:imported ? TASKNAME : '',
            headers:typeof TASKHEADERS !== 'undefined' ? TASKHEADERS : []};
  }
  async function refresh() {
    const id=++previewId;
    preview=null;
    byId('write').disabled=true;
    byId('zip').disabled=true;
    try {
      const plan=await prepare(inputs());
      if (id!==previewId) return;
      preview=plan;
      const entries = [...plan.files,...(plan.menu ? [plan.menu] : [])];
      byId('files').replaceChildren(...entries.map(f=>{
        const li=document.createElement('li');
        li.textContent=(f.exists ? (f.changed?'更新  ':'相同  ') : '新建  ')+f.path;
        return li;
      }));
      const locked = plan.files.some(f=>f.exists && f.changed) && !byId('overwrite').checked;
      byId('write').disabled = !project || locked;
      byId('zip').disabled=false;
      byId('status').textContent=[...plan.warnings,...(locked ? ['勾选「更新已有文件」后可写入工程。'] : [])].join('\n');
    } catch(err) {
      if (id!==previewId) return;
      byId('files').replaceChildren();
      byId('status').textContent=err.message;
      if (project && byId('register').checked) {
        try {
          await prepare(inputs(true));
          if (id===previewId) byId('zip').disabled=false;
        } catch { /* The same design cannot be exported without registration either. */ }
      }
    }
  }
  function lockPanel(on) {
    busy=on;
    panel.querySelectorAll('button,input').forEach(el=>{el.disabled=on;});
    if (!on) byId('register').disabled=!project;
  }
  function initPanel() {
    const style=document.createElement('style');
    style.textContent=
      '#dtg-panel{box-sizing:border-box;width:min(530px,95vw);max-height:92vh;overflow:auto;padding:22px;border:1px solid #40536a;border-radius:10px;background:#1a2634;color:#e7eff7;font:14px/1.5 system-ui,sans-serif}'+
      '#dtg-panel::backdrop{background:#101924aa}#dtg-panel h2{font-size:18px;margin:0}#dtg-panel header,#dtg-panel footer,#dtg-panel .dtg-row{display:flex;align-items:center;gap:10px;flex-wrap:wrap}'+
      '#dtg-panel header{justify-content:space-between;margin-bottom:16px}#dtg-panel footer{margin-top:16px}#dtg-panel input[type=text]{box-sizing:border-box;width:100%;padding:8px;margin:3px 0 12px;color:#f1f6fb;background:#101a26;border:1px solid #475d71;border-radius:4px}'+
      '#dtg-panel button{padding:6px 11px;font:inherit;cursor:pointer;background:#263a50;color:#edf5fc;border:1px solid #536980;border-radius:4px}#dtg-panel button:disabled{opacity:.45;cursor:default}'+
      '#dtg-panel #dtg-write{background:#185d9d}#dtg-panel label{display:block;margin:7px 0}#dtg-panel input[type=checkbox]{width:auto;vertical-align:middle;margin-right:5px}'+
      '#dtg-panel ul{padding:10px 12px;margin:12px 0;background:#101b27;border-radius:4px;list-style:none;overflow-wrap:anywhere;font:12px/1.8 Consolas,monospace}#dtg-panel ul:empty{display:none}'+
      '#dtg-status{white-space:pre-wrap;overflow-wrap:anywhere;color:#d8e5ee}';
    document.head.appendChild(style);
    panel=document.createElement('dialog'); panel.id='dtg-panel';
    panel.setAttribute('aria-labelledby','dtg-title');
    panel.innerHTML='<header><h2 id="dtg-title">生成任务三件套</h2><button type="button" id="dtg-close">关闭</button></header>'+
      '<label for="dtg-name">任务名</label><input type="text" id="dtg-name" autocomplete="off" spellcheck="false" placeholder="task_light">'+
      '<div class="dtg-row"><button type="button" id="dtg-project">选择工程根目录</button><span id="dtg-root">未选择工程</span></div>'+
      '<label><input type="checkbox" id="dtg-register">写入工程时登记到主菜单</label>'+
      '<label><input type="checkbox" id="dtg-overwrite">更新已有文件（保留任务业务逻辑）</label>'+
      '<ul id="dtg-files"></ul><div id="dtg-status" role="status"></div>'+
      '<footer><button type="button" id="dtg-zip">下载三件套 ZIP</button><button type="button" id="dtg-write">写入工程</button></footer>';
    document.body.appendChild(panel);
    panel.addEventListener('cancel',e=>{if(busy)e.preventDefault();});
    byId('close').onclick=()=>panel.close();
    byId('name').oninput=refresh;
    byId('register').onchange=refresh;
    byId('overwrite').onchange=refresh;
    byId('project').onclick=async()=>{
      if (!globalThis.showDirectoryPicker) {
        byId('status').textContent='当前浏览器无法直接写工程，请下载 ZIP，或用桌面 Edge / Chrome 打开此工具。';
        return;
      }
      try {
        const picked=await showDirectoryPicker({mode:'readwrite',id:'task-designer-target'});
        if (!(await read(picked,'User/tasks.h'))) throw new Error('请选择含 User/tasks.h 的工程根目录。');
        const config=await read(picked,'LVGL/GUI/lvgl/lv_conf.h');
        setWidgetConfig(config ? new TextDecoder('windows-1252').decode(config) : '');
        project=picked; byId('root').textContent=picked.name;
        byId('register').disabled=false;
        byId('overwrite').checked=false;
        await refresh();
      } catch(err) {if(err.name!=='AbortError')byId('status').textContent=err.message;}
    };
    byId('zip').onclick=async()=>{
      try {
        const plan=await prepare(inputs(true));
        const url=URL.createObjectURL(zip(plan.files));
        const a=document.createElement('a'); a.href=url; a.download=plan.name+'.zip'; a.click();
        setTimeout(()=>URL.revokeObjectURL(url),3000);
        byId('status').textContent=['已生成 '+plan.name+'.zip，解压到工程根目录即可。ZIP 中包含上述三个任务文件。',...plan.warnings].join('\n');
      } catch(err) {byId('status').textContent=err.message;}
    };
    byId('write').onclick=async()=>{
      const plan=preview;
      if (!plan || !project || busy) return;
      const overwrite=byId('overwrite').checked;
      lockPanel(true);
      let message;
      try {
        const written=await write(plan,{overwrite});
        message=written.length ? '已写入：\n'+written.join('\n') : '文件内容一致，无需写入。';
        if (plan.warnings.length) message+='\n'+plan.warnings.join('\n');
      } catch(err) {message=err.message;}
      finally {lockPanel(false); await refresh(); byId('status').textContent=message;}
    };
  }
  function open() {
    if (!panel) initPanel();
    const imported=typeof TASKNAME!=='undefined' && /^task_[a-z0-9_]+\.c$/.test(TASKNAME);
    const prefix=(document.getElementById('projName')?.value || '').replace(/_ui$/,'');
    byId('name').value=imported ? TASKNAME.slice(0,-2) : (normalize(prefix) || 'task_light');
    byId('register').disabled=!project;
    byId('overwrite').checked=false;
    if (!panel.open) panel.showModal();
    refresh();
  }
  globalThis.DesignerTaskGenerator={open, normalize, prepare, write, zip, encodeGBK};
})();
