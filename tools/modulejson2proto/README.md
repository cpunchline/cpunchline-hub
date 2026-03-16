# modulejson2proto.py - 模块json结构转proto工具

## 模块json语法

文件名: `XXX_module.json`, 其中`XXX`为模块(服务提供者)名称;

### json-Object: Module

该`json对象`描述模块(服务提供者)的信息;
生成的代码文件名均遵循`XXX_auto.pb.h/XXX_auto.pb.c`规则;

* `name`: 模块名称; 自动转大写;
* `id`: 模块ID;

### json-Array: ProviderService

该`json数组`描述模块(服务提供者)提供的服务及相关信息;
生成代码文件中服务的枚举定义遵循`大写XXX_SERVICE_ID_TYPE_NAME`;

* `type`: 服务类型; 大写;
    * `EVENT` 通知/广播模式; 一定没有出参;
    * `METHOD` 请求-应答模式;
* `name`: 服务名; 自动转大写;
* `in`: 入参;
* `out`: 出参;

### json-Object: EnumDef
``` json
        "ENUM_NAME": {
            "enum_member1": 0,
            "enum_member2": -1
        }
```

* `ENUM_NAME`: 自定义枚举名; 自动转大写;
* `enum_member1` `enum_member2`: 自定义枚举变量名; 对应键值为枚举值; 自动转大写;

### json-Object: UnionDef

预定义联合体

``` json
        "un_union_name": {
            "union_member1": "int32",
            "union_member2": "string"
            // ...
        }
```
* `un_union_name`: 自定义联合体名; 注意最终生成出来的联合体并不是语法上的等价联合体, 而是一个结构体套了一个联合体(`oneof语法的限制`);
* `union_member1` `union_member2`: 自定义联合体成员名; 对应键值为联合体成员类型; 
    * 注意: 如果联合体成员类型需要定义为结构体`array`, 需要单独先将该结构体`array`定义为结构体, 然后将该结构体定义为联合体成员(`oneof语法的限制`);

### json-Object: StructDef

预定义结构体

``` json
        "st_struct_name": {
            "struct_member": "struct_member_type",
            "struct_member_array": {
                "array": {
                    "uint32": 10
                }
            }
            // ...
        }
```

* `st_struct_name`: 自定义结构体名; 建议为小写;
* `struct_member`: 自定义结构体成员名; 建议为小写;
    * 结构体成员名不能定义为`array`;
* `struct_member_type`: 自定义结构体成员类型;
    * `bool`: bool
    * `string`: char
    * `bytes`: uint8_t
    * `int32`: int32_t
    * `uint32`: uint32_t
    * `int64`: int64_t
    * `uint64`: uint64_t
    * `float`: float
    * `double`: double
    * `StructDef`: 预定义结构体;
    * `UnionDef`: 预定义联合体;
    * `Array`: 如结构体成员`struct_member_array`, 其内部定义对象`array`(名称固定), `array`内部的`uint32`表示数组类型, `10`表示数组长度;
