const fs = require('fs');
const path = require('path');

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
 * 处理proto文件内容解析
 * @param {string} protoContent proto文件文本内容
 * @returns 返回解析出的内容
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

  // 按行分割文件文本内容
  const lines = protoContent.split('\n');

  // 处理按行解析
  lines.forEach(rawLine => {
    // 去掉行开头与末尾的空格
    const line = rawLine.trim();

    // 空行则跳过，但清空待处理注释
    if (!line || line === "") {
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
    // 提取行开头为 ^message 
    // \s 匹配任何空白字符（如空格、制表符、换行符） + 匹配1个或多个
    // (\w+) 匹配一个字符、数字或者下划线等价于[a-zA-Z0-9_] +表示1个或多个空白字符 ()表示一个捕获组这部分内容会被提取出来，作为匹配结果的一部分返回
    const msgMatch = line.match(/^message\s+(\w+)/);
    if (msgMatch) {
      pendingMessage = msgMatch[1]; // mesgMatch[0]为全匹配 msgMatch[1]为()捕获部分
      console.debug(`pendingMessage: ${pendingMessage}`);
      return;
    }

    // 提取自定义enum 与 message方法相同
    const enumMatch = line.match(/^enum\s+(\w+)/);
    if (enumMatch) {
      pendingEnum = enumMatch[1];
      console.debug(`pendingEnum: ${pendingEnum}`);
      return;
    }

    // 协议开头{的那行
    if (line === '{') {
      // 正在处理 message内的oneof代码块
      if (currentOneof) {
        return;
      }

      // 这是个message
      if (pendingMessage) {
        // 将但当前正在处理的message状态进行切换
        currentMessage = pendingMessage;
        messages[currentMessage] = {
          fields: [],
          comment: pendingComments.join(' ') // 保存 message 的注释
        };
        pendingMessage = null;
        pendingComments = [];
        return;
      }

      // 这是个enum
      if (pendingEnum) {
        // 将但当前正在处理的enum状态进行切换
        currentEnum = pendingEnum;
        enums[currentEnum] = {
          values: [],
          comment: pendingComments.join(' ') // 保存 enum 的注释
        };
        pendingEnum = null;
        pendingComments = [];
        return;
      }
    }

    // 目前正在处理的是enum 按照枚举字段处理
    if (currentEnum) {
      // like
      // ENUM_ITEM = 1;
      // ENUM_ITEM = 2;
      // ENUM_ITEM = 3 ;
      const enumItem = line.match(/^(\w+)\s*=\s*(\d+)\s*;/);
      if (enumItem) {
        enums[currentEnum].values.push({
          name: enumItem[1],
          value: enumItem[2],
          comment: pendingComments.join(' ') // 保存枚举值的注释
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
      return;
    }

    // 目前正在处理 message内的oneof 字段代码块
    if (currentOneof && currentMessage) {
      // oneof代码块内的字段
      const oneofField = line.match(/^(\w+)\s+(\w+)\s*=\s*\d+;/);
      if (oneofField) {
        messages[currentMessage].push({
          fieldName: oneofField[2], // 字段名
          protoType: oneofField[1],
          luaType: protoToLuaType(oneofField[1]), // 字段对应lua类型
          oneof: currentOneof, // 是否是message内的oneof字段
          comment: pendingComments.join(' ') // 保存字段注释
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
        comment: pendingComments.join(' ') // 保存字段注释
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
          comment: pendingComments.join(' ') // 保存字段注释
        });
        pendingComments = [];
        return;
      }
    }

    // 代码块结尾
    if (line === '}') {
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
    // 输出枚举的注释
    if (enumData.comment) {
      out += `--- ${enumData.comment}\n`;
    }
    out += `---@alias ${PREFIX}${name} table<string,integer>\n`;
    out += `${PREFIX}${name} = {\n`;

    enumData.values.forEach(v => {
      // 输出枚举值得注释
      if (v.comment) {
        out += `  --- ${v.comment}\n`;
      }
      out += `  ${v.name} = ${v.value},\n`;
    });
    out += `}\n\n`;
  }

  // message类型
  for (const [msg, msgData] of Object.entries(messages)) {
    // 输出 message 的注释
    if (msgData.comment) {
      out += `--- ${msgData.comment}\n`;
    }

    out += `---@class ${PREFIX}${msg}\n`;

    msgData.fields.forEach(f => {
      // 构建注释部分
      let commentPart = f.protoType;
      if (f.comment) {
        commentPart += ` ${f.comment}`;
      }

      // oneof字段
      if (f.oneof) {
        out += `---@field ${f.fieldName} ${f.luaType}|nil ${commentPart} -- oneof ${f.oneof}\n`;
      }
      else if (f.repeated) {
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
 * @param {*} protoFile 要处理的文件路径
 * @param {*} outDir 输出路径
 * @param {*} 是否需要require内部含有枚举定义
 */
function processProtoFile(protoFile, outDir, protoImport) {
  // 同步读出输入文件全部文本内容
  const protoContent = fs.readFileSync(protoFile, 'utf8');
  // 解析proto文件内容
  const { messages, enums } = parseProto(protoContent);
  // console.log("messages=>", JSON.stringify(messages));
  // console.log("enums=>", JSON.stringify(enums));

  // 生成lua文件内容
  const luaCode = generateLuaClasses(messages, enums);
  if (luaCode === '') {
    console.error(`生成失败: generateLuaClasses empty protoFile:${protoFile} outDir:${outDir}`);
    process.exit(1);
  }

  // 获取文件名且去掉扩展名
  let baseName = path.basename(protoFile, '.proto');
  baseName = toProtoFormat(baseName);
  // 拼接输出文件路径和.lua文件名称
  const outFile = path.join(outDir, `${baseName}.lua`);
  // 同步写目标lua文件
  fs.writeFileSync(outFile, luaCode);

  if (Object.entries(enums).length > 0) {
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

  // 这里inputPath或outDir仍没指定肯定有问题
  if (!inputPath || !outDir) {
    console.error('用法: node generate_lua.js <proto文件或目录> <输出目录>');
    process.exit(1);
  }

  if (!fs.existsSync(inputPath)) {
    console.error('找不到输入路径:', inputPath);
    process.exit(1);
  }

  // 检查输出目标目录是否存在不存在则创建
  if (!fs.existsSync(outDir)) {
    try {
      fs.mkdirSync(outDir, { recursive: true });
    } catch (err) {
      console.error(`创建目录失败 ${err}`);
      process.exit(1);
    }
  }

  // 读取inputPath类型 可能是目录也可能是文件
  const stat = fs.statSync(inputPath);

  let protoImport = [];

  // 单个proto文件
  if (stat.isFile()) {
    if (!inputPath.endsWith('.proto')) {
      console.error(`输入文件不是 .proto: ${inputPath}`);
      process.exit(1);
    }

    // 只处理单个proto文件
    processProtoFile(inputPath, outDir, protoImport);

  }
  else if (stat.isDirectory()) {  // 处理输入文件夹下的proto文件

    // 读取文件夹
    const files = fs.readdirSync(inputPath);

    files.forEach(file => {
      // 只处理文件夹下的proto文件
      if (!file.endsWith('.proto')) return;

      // 拼接路径
      const fullPath = path.join(inputPath, file);

      // 处理单个文件
      processProtoFile(fullPath, outDir, protoImport);
    });


  } else {
    console.error('输入路径既不是文件也不是目录:', inputPath);
    process.exit(1);
  }

  console.log("内部含有枚举的Lua文件 ", protoImport);

  console.log("生成ProtoLuaImport")

  {
    let protoLuaImportStr = "";
    for (let baseName of protoImport) {
      protoLuaImportStr += `require("${baseName}");\n`;
    }
    fs.writeFileSync(outDir + "/ProtoLuaImport.lua", protoLuaImportStr);
  }

  console.log('处理完成');

}

main();
