const {
  existsSync,
  statSync,
  readdirSync,
  readFileSync,
  writeFileSync,
  mkdirSync,
} = require('fs');
const { join, basename } = require('path');

const PREFIX = `ProtoLua_`;

/**
 * proto 字段类型转lua类型
 * @param {string} protoType proto字段类型
 * @param {isOneOf} boolean 是否oneof字段
 * @param {isRepeated} boolean 是否repeated字段
 * @returns lua类型
 */
function protoToLuaType(protoType, isOneOf, isRepeated) {
  const typeMap = {
    int32: "integer",
    int64: "string",
    uint32: "integer",
    uint64: "string",
    float: "number",
    double: "number",
    bool: "boolean",
    string: "string",
    bytes: "string",
  };

  // 普通类型
  if (typeMap[protoType]) {
    return typeMap[protoType];
  }

  // 自定义Message类型
  return `${PREFIX}${protoType}`;
}

/**
 * 判断一行在去掉声明头部之后，剩余部分是否就是 `{`（忽略行尾注释）
 * 用于兼容 `message Foo {` / `enum Foo {` 等单行写法
 * @param {string} line 去除首尾空格后的行内容
 * @param {string} head 声明头部文本（如 `message Foo`）
 * @returns {boolean} 声明头部之后是否紧跟 {
 */
function hasOpenBraceAfter(line, head) {
  const rest = line.slice(head.length).replace(/\/\/.*$/, '').trim();
  return rest === '{';
}

/**
 * 将注释数组逐行渲染为 EmmyLua 的 `---` 注释
 * @param {string[]} comments 注释数组（每行注释一个元素）
 * @param {string} indent 每行的缩进前缀
 * @returns 渲染后的注释文本（以 \n 结尾，可直接拼接）
 */
function renderComments(comments, indent = '') {
  let s = '';
  for (const c of comments) {
    // 空注释行（如裸 // ）输出为不带尾巴的 ---，避免行尾空格
    s += `${indent}---${c ? ` ${c}` : ''}\n`;
  }
  return s;
}

/**
 * 处理proto文件内容解析
 * @param {string} protoContent proto文件文本内容
 * @returns {{ messages: object, enums: object }} 解析出的内容
 *   message 形状: { fields: Array, comment: string[] }
 *   enum 形状:    { values: Array, comment: string[] }
 *   field 形状:   { fieldName, protoType, luaType, comment: string[], oneof?, repeated? }
 *   其中 comment 为数组，每行注释一个元素（无注释时为空数组）
 */
function parseProto(protoContent) {
  const messages = {};
  const enums = {};

  let currentMessage = null;
  let currentEnum = null;
  let currentOneof = null;

  let pendingMessage = null;
  let pendingEnum = null;
  // 收集注释
  let pendingComments = [];

  // 打开当前 message / enum 的代码块
  const openBlock = () => {
    // 正在处理 message内的oneof代码块，oneof不需要 { 开头
    if (currentOneof) {
      return;
    }

    // 这是个message
    if (pendingMessage) {
      currentMessage = pendingMessage;
      messages[currentMessage] = {
        fields: [],
        comment: [...pendingComments] // 保存 message 的注释（数组，每行注释一个元素）
      };
      pendingMessage = null;
      pendingComments = [];
      return;
    }

    // 这是个enum
    if (pendingEnum) {
      currentEnum = pendingEnum;
      enums[currentEnum] = {
        values: [],
        comment: [...pendingComments] // 保存 enum 的注释（数组，每行注释一个元素）
      };
      pendingEnum = null;
      pendingComments = [];
    }
  };

  // 按行分割文件文本内容
  const protoFileContentLines = protoContent.split('\n');

  // 处理按行解析
  protoFileContentLines.forEach(rawLine => {
    // 去掉行开头与末尾的空格
    const line = rawLine.trim();

    // 空行则跳过
    if (!line) {
      return;
    }

    // 收集注释行
    if (line.startsWith('//')) {
      const comment = line.replace(/^\/\/\s*/, ''); // 去掉 // 和后面的空格
      pendingComments.push(comment);
      return;
    }

    // 如果行开头是某些特殊的关键词则跳过处理行
    if (
      line.startsWith('syntax') ||
      line.startsWith('package') ||
      line.startsWith('option')
    ) {
      pendingComments = []; // 清空注释
      return;
    }

    // 提取自定义message
    // ^message\s+(\w+) 匹配 message 声明行，(\w+) 捕获组提取名称
    // 兼容 `message Foo {` 这种名称后直接跟 { 的单行写法
    const msgMatch = line.match(/^message\s+(\w+)/);
    if (msgMatch) {
      pendingMessage = msgMatch[1];
      if (hasOpenBraceAfter(line, msgMatch[0])) {
        openBlock();
      }
      return;
    }

    // 提取自定义enum 与 message方法相同
    const enumMatch = line.match(/^enum\s+(\w+)/);
    if (enumMatch) {
      pendingEnum = enumMatch[1];
      if (hasOpenBraceAfter(line, enumMatch[0])) {
        openBlock();
      }
      return;
    }

    // 协议开头{的那行
    if (line === '{') {
      openBlock();
    }

    // 目前正在处理的是enum 按照枚举字段处理
    if (currentEnum) {
      // like
      // ENUM_ITEM = 1;
      // ENUM_ITEM = -1; // 兼容负数与行尾注释
      const enumItem = line.match(/^(\w+)\s*=\s*(-?\d+)\s*;\s*(?:\/\/.*)?$/);
      if (enumItem) {
        enums[currentEnum].values.push({
          name: enumItem[1],
          value: enumItem[2],
          comment: [...pendingComments] // 保存枚举值的注释（数组，每行注释一个元素）
        });
        pendingComments = [];
        return;
      }
    }

    // 处理开头 如 oneof my_field的行
    const oneofMatch = line.match(/^oneof\s+(\w+)/);
    if (oneofMatch && currentMessage) {
      currentOneof = oneofMatch[1];
      pendingComments = [];

      // 兼容 `oneof x { ... }` 全部写在同一行的写法：
      // 直接解析行内的字段并关闭 oneof，避免后面的普通字段被误判为 oneof
      const rest = line.slice(oneofMatch[0].length);
      if (rest.includes('{') && rest.includes('}')) {
        for (const m of rest.matchAll(/(\w+)\s+(\w+)\s*=\s*\d+;/g)) {
          messages[currentMessage].fields.push({
            fieldName: m[2],
            protoType: m[1],
            luaType: protoToLuaType(m[1]),
            oneof: currentOneof,
            comment: []
          });
        }
        currentOneof = null;
      }
      return;
    }

    // 目前正在处理 message 内的 oneof 字段代码块
    if (currentOneof && currentMessage) {
      // oneof代码块内的字段
      const oneofField = line.match(/^(\w+)\s+(\w+)\s*=\s*\d+;/);
      if (oneofField) {
        messages[currentMessage].fields.push({
          fieldName: oneofField[2], // 字段名
          protoType: oneofField[1],
          luaType: protoToLuaType(oneofField[1]), // 字段对应lua类型
          oneof: currentOneof, // 是否是message内的oneof字段
          comment: [...pendingComments] // 保存字段注释（数组，每行注释一个元素）
        });
        pendingComments = [];
        return;
      }
    }

    // message内的repeated字段
    const repeatedMatch = line.match(
      /^repeated\s+(\w+)\s+(\w+)\s*=\s*\d+/
    );
    if (repeatedMatch && currentMessage) {
      const fieldType = repeatedMatch[1];
      const fieldName = repeatedMatch[2];
      const luaType = protoToLuaType(fieldType);

      messages[currentMessage].fields.push({
        fieldName,
        protoType: fieldType,
        luaType: `table<integer,${luaType}>`, // repeated字段按lua数组处理
        repeated: true,
        comment: [...pendingComments] // 保存字段注释（数组，每行注释一个元素）
      });
      pendingComments = [];
      return;
    }

    // message内普通字段
    if (!currentOneof && currentMessage) {
      const fieldMatch = line.match(/^(\w+)\s+(\w+)\s*=\s*\d+/);

      if (fieldMatch) {
        messages[currentMessage].fields.push({
          fieldName: fieldMatch[2],
          protoType: fieldMatch[1],
          luaType: protoToLuaType(fieldMatch[1]),
          comment: [...pendingComments] // 保存字段注释（数组，每行注释一个元素）
        });
        pendingComments = [];
        return;
      }
    }

    // 代码块结尾（兼容 `};` 或 `} // 注释` 写法）
    if (/^};?\s*(?:\/\/.*)?$/.test(line)) {
      // oneof肯定在 message代码块外优先关闭代码块
      if (currentOneof) {
        currentOneof = null;
      } else if (currentEnum) {
        currentEnum = null;
      } else if (currentMessage) {
        currentMessage = null;
      }
      pendingComments = [];
    }
  });

  return { messages, enums };
}

/**
 * 生成EmmyLua类型约束注释
 * @param {*} messages parseProto 解析生成的 messages
 * @param {*} enums parseProto 解析生成的 enums
 * @returns lua文件文本内容
 */
function generateLuaClasses(messages, enums) {
  let out = '';

  // 处理每个枚举类型
  for (const [name, enumData] of Object.entries(enums)) {
    // 输出枚举的注释（每行注释各占一条 ---）
    if (enumData.comment.length) {
      out += renderComments(enumData.comment);
    }
    out += `---@alias ${PREFIX}${name} table<string,integer>\n`;
    out += `${PREFIX}${name} = {\n`;

    enumData.values.forEach(v => {
      // 输出枚举值的注释（每行注释各占一条 ---）
      if (v.comment.length) {
        out += renderComments(v.comment, '  ');
      }
      out += `    ${v.name} = ${v.value},\n`;
    });
    out += `}\n\n`;
  }

  // message类型
  for (const [msg, msgData] of Object.entries(messages)) {
    // 输出 message 的注释（每行注释各占一条 ---）
    if (msgData.comment.length) {
      out += renderComments(msgData.comment);
    }

    out += `---@class ${PREFIX}${msg}\n`;

    msgData.fields.forEach(f => {
      // 字段注释多行时，除最后一行外各占一条 ---，最后一行保持内联（单行注释输出与原来一致）
      if (f.comment.length > 1) {
        out += renderComments(f.comment.slice(0, -1));
      }

      // 构建注释部分
      let commentPart = f.protoType;
      const lastComment = f.comment[f.comment.length - 1];
      if (lastComment) {
        commentPart += ` ${lastComment}`;
      }

      // oneof字段
      if (f.oneof) {
        out += `---@field ${f.fieldName} ${f.luaType}|nil ${commentPart} -- oneof ${f.oneof}\n`;
      }
      else if (f.repeated) { // 数组字段
        out += `---@field ${f.fieldName} ${f.luaType} repeated ${commentPart}\n`;
      }
      else { // 非oneof字段
        out += `---@field ${f.fieldName} ${f.luaType} ${commentPart}\n`;
      }
    });
    out += `\n`;
  }

  return out;
}

/**
 * 将 str 从 proto_message_head 转为 ProtoLuaMessageHead 返回
 * @param {*} str 形如 proto_message_head
 * @returns 形如 ProtoLuaMessageHead
 */
function toProtoFormat(str) {
  // 替换开头的 'proto' 为 'ProtoLua'
  if (str.startsWith('proto_')) {
    str = 'ProtoLua' + str.slice(5);  // 去掉 'proto_' 前缀，替换为 'ProtoLua'
  }

  return str
    .split('_')   // 按下划线分隔
    .map((word, index) => {
      // 将每个单词首字母大写，其他字母小写
      if (index === 0) {
        return word.charAt(0).toUpperCase() + word.slice(1);  // 第一个字母大写
      } else {
        return word.charAt(0).toUpperCase() + word.slice(1).toLowerCase();
      }
    })
    .join('');  // 连接成一个字符串
}

/**
 * 处理单个 proto 文件
 * @param {*} protoFileFullPath 要处理的文件路径
 * @param {*} outDir 输出路径
 * @param {*} protoImport 如果proto文件内部有定义枚举，则将生成的lua文件名追加到protoImport列表中
 */
function processProtoFile(protoFileFullPath, outDir, protoImport) {
  // 同步读出输入文件全部文本内容（去掉可能存在的BOM）
  const protoContent = readFileSync(protoFileFullPath, 'utf8').replace(/^\uFEFF/, '');
  // 解析proto文件内容
  const { messages, enums } = parseProto(protoContent);
  // console.log("messages=>", JSON.stringify(messages));
  // console.log("enums=>", JSON.stringify(enums));

  // 生成lua文件内容
  const luaCode = generateLuaClasses(messages, enums);
  if (luaCode === '') {
    console.error(`生成失败: generateLuaClasses empty protoFile:${protoFileFullPath} outDir:${outDir}`);
    process.exit(1);
  }

  // 获取文件名且去掉扩展名
  let baseName = basename(protoFileFullPath, '.proto');
  baseName = toProtoFormat(baseName);
  // 拼接输出文件路径和.lua文件名称
  const outFile = join(outDir, `${baseName}.lua`);
  // 同步写目标lua文件
  writeFileSync(outFile, luaCode);

  if (Object.keys(enums).length > 0) {
    protoImport.push(baseName);
  }

  console.log(`生成成功: ${outFile}`);
}

// node generate_lua.js <proto文件或目录> <输出目录>
function main() {
  // node generate_lua.js inputPath outDir
  let [, , inputPath, outDir] = process.argv;

  // 默认使用 inputPath=./protocol/ outDir=./lua/ProtoLua/
  if (!inputPath || !outDir) {
    inputPath = './protocol/';
    outDir = './lua/ProtoLua/';
    console.warn(`using default inputPath=./protocol/ outDir=./lua/ProtoLua/`);
  }

  if (!existsSync(inputPath)) {
    console.error('找不到输入路径:', inputPath);
    process.exit(1);
  }

  // 检查输出目标目录是否存在不存在则创建
  if (!existsSync(outDir)) {
    try {
      mkdirSync(outDir, { recursive: true });
    } catch (err) {
      console.error(`创建目录失败 ${err}`);
      process.exit(1);
    }
  }

  // 读取inputPath类型 可能是目录也可能是文件
  const inputPathStat = statSync(inputPath);

  let protoImport = [];

  // 单个proto文件
  if (inputPathStat.isFile()) {
    if (!inputPath.endsWith('.proto')) {
      console.error(`输入文件不是 .proto: ${inputPath}`);
      process.exit(1);
    }

    // 只处理单个proto文件
    processProtoFile(inputPath, outDir, protoImport);

  }
  else if (inputPathStat.isDirectory()) {  // 处理输入文件夹下的proto文件

    // 读取文件夹（排序保证输出与 require 顺序稳定）
    const files = readdirSync(inputPath).sort();

    files.forEach(file => {
      // 只处理文件夹下的proto文件
      if (!file.endsWith('.proto')) return;

      // 拼接路径
      const protoFileFullPath = join(inputPath, file);
      console.log("Prepare to process with processProtoFile:", protoFileFullPath);

      // 处理单个文件
      processProtoFile(protoFileFullPath, outDir, protoImport);
    });


  } else {
    console.error('输入路径既不是文件也不是目录:', inputPath);
    process.exit(1);
  }

  console.log("内部含有枚举的Lua文件 ", protoImport);

  console.log("Prepare to gennerate: ProtoLuaImport.lua")

  {
    let protoLuaImportStr = "";
    for (let baseName of protoImport) {
      protoLuaImportStr += `require("${baseName}");\n`;
    }
    writeFileSync(join(outDir, "ProtoLuaImport.lua"), protoLuaImportStr);
  }

  console.log('处理完成');

}

main();
