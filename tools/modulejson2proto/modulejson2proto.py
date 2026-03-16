#!/usr/bin/env python3

import sys
import shutil
import json
from pathlib import Path
from collections import OrderedDict

'''
sudo apt install python3-openpyxl
'''

class bcolors:
    HEADER   ='\033[95m'  # 亮洋红(Magenta)     ← 用于标题/分隔线
    BLUE     ='\033[94m'  # 亮蓝色(Blue)        ← 用于普通信息,路径,URL
    YELLOW   ='\033[93m'  # 亮黄色(Yellow)      ← 用于警告,注意
    GREEN    ='\033[92m'  # 亮绿色(Green)       ← 用于成功,完成,确认
    RED      ='\033[91m'  # 亮红色(Red)         ← 用于错误,失败,危险操作
    ENDC     ='\033[0m'   # 重置所有样式         ← 必须用它关闭颜色!
    BOLD     ='\033[1m'   # 粗体(Bold)          ← 强调关键内容
    UNDERLINE='\033[4m'   # 下划线(Underline)   ← 较少用,可用于链接或重点

def util_print(text, color):
    print(f'{color}-- {text}{bcolors.ENDC}')

def util_is_empty(x):
    return x is None or (isinstance(x, str) and x.strip() == "")

def util_prepare_paths(jsonpath: str, generatorprotopath: str):
    # path -> abs path
    jsonpath=Path(jsonpath).resolve()
    generatorprotopath=Path(generatorprotopath).resolve()
    if generatorprotopath.exists():
        shutil.rmtree(generatorprotopath)
    generatorprotopath.mkdir(parents=True, exist_ok=False)

    return jsonpath, generatorprotopath

def util_find_jsonfiles(input_path: str):
    if Path(input_path).is_dir():
        json_files=list(Path(input_path).rglob('*.json'))
        if not json_files:
            util_print(f'Warning: No .json files found in directory: {input_path}', bcolors.YELLOW)
        return json_files
    else:
        util_print(f'Error: Input path does not exist: {input_path}', bcolors.RED)
        sys.exit(1)

def util_open_proto_file(generatorprotopath: str, module_name: str):
    proto_file_name=generatorprotopath+'/'+module_name+'_module.proto'
    proto_file=open(proto_file_name, 'w+')
    proto_file.write('syntax = \'proto3\';\n')
    proto_file.write('/* ')
    proto_file.write(module_name)
    proto_file.write(' */\n\n')
    return proto_file

def util_open_protooption_file(generatorprotopath: str, module_name: str):
    protooption_file_name=generatorprotopath+'/'+module_name+'_module.options'
    protooption_file=open(protooption_file_name, 'w+')
    return protooption_file

def util_get_array_type(member_type):
    array_type=None
    try:
        member_array=member_type['array']
        for key in member_array.keys():
            array_type=key
    except TypeError:
        array_type=None
    return array_type

def util_file_process(proto_file, protooption_file, params, struct_name: str):
    spacing_symbol='    '
    for i in range(0, len(params)):
        spacing_symbol='    '
        if params[i][0] != 'string' and params[i][0] != 'bytes':
            if params[i][2] != None:
                proto_file.write('    repeated')
                protooption_file.write(str(struct_name)+'.'+str(params[i][1])+' max_count:'+str(params[i][2])+'\n')
                spacing_symbol=' '
        else:
            protooption_file.write(str(struct_name)+'.'+str(params[i][1])+' max_size:'+str(params[i][2])+'\n')
        proto_file.write(spacing_symbol+params[i][0]+' '+params[i][1]+' = '+str(i+1)+';\n')

def util_generator_autolib_h_c_begin(generatorprotopath: Path):
    autolib_def_h_file=str(generatorprotopath)+'/'+'generator_autolib_def.h'
    autolib_def_h_file_content='''
#ifndef __GENERATOR_AUTOLIB_DEF_H__
#define __GENERATOR_AUTOLIB_DEF_H__

#ifdef __cplusplus
extern "C"
{
#endif

'''
    with open(autolib_def_h_file, 'w+') as f:
        f.write(autolib_def_h_file_content)

    autolib_h_file=str(generatorprotopath)+'/'+'generator_autolib.h'
    autolib_h_file_content='''
#ifndef __GENERATOR_AUTOLIB_H__
#define __GENERATOR_AUTOLIB_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "pb.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    uint32_t service_id;
    const pb_msgdesc_t *in_fields;
    const uint32_t in_size;
    const pb_msgdesc_t *out_fields;
    const uint32_t out_size;
} st_autolib_servicemap_item;

typedef struct
{
    const st_autolib_servicemap_item *items;
    const uint32_t items_size;
} st_autolib_servicemap;

#include "generator_autolib_def.h"

extern st_autolib_servicemap* gst_autolib_servicemap[];

'''

    with open(autolib_h_file, 'w+') as f:
        f.write(autolib_h_file_content)

    autolib_c_file=str(generatorprotopath)+'/'+'generator_autolib.c'
    autolib_c_file_content='''
#include "generator_autolib.h"

st_autolib_servicemap* gst_autolib_servicemap[] =
{
'''
    with open(autolib_c_file, 'w+') as f:
        f.write(autolib_c_file_content)

def util_generator_autolib_h_c_handle(generatorprotopath: Path, module_name: str, service_list: list, module_id: int):
    autolib_def_h_file=str(generatorprotopath)+'/'+'generator_autolib_def.h'
    autolib_def_h_file_content='#include \"'+module_name+'_module.pb.h\"\n'
    with open(autolib_def_h_file, 'a+') as f:
        f.write(autolib_def_h_file_content)

    autolib_h_file=str(generatorprotopath)+'/'+'generator_autolib.h'
    autolib_h_file_content='#define MODULE_ID_AUTOLIB_'+str(module_name).upper()+' ('+str(module_id)+')\n'
    autolib_h_file_content=autolib_h_file_content+'extern st_autolib_servicemap gst_'+str(module_name)+'_autolib_servicemap;\n'
    with open(autolib_h_file, 'a+') as f:
        f.write(autolib_h_file_content)

    autolib_c_file=str(generatorprotopath)+'/'+'generator_autolib.c'
    autolib_c_file_content='    [MODULE_ID_AUTOLIB_'+str(module_name).upper()+'] = &gst_'+str(module_name)+'_autolib_servicemap,\n'
    with open(autolib_c_file, 'a+') as f:
        f.write(autolib_c_file_content)

def util_generator_autolib_h_c_end(generatorprotopath: Path):
    autolib_def_h_file=str(generatorprotopath)+'/'+'generator_autolib_def.h'
    autolib_def_h_file_content='''
#ifdef __cplusplus
}
#endif

#endif // __GENERATOR_AUTOLIB_DEF_H__
'''
    with open(autolib_def_h_file, 'a+') as f:
        f.write(autolib_def_h_file_content)

    autolib_h_file=str(generatorprotopath)+'/'+'generator_autolib.h'
    autolib_h_file_content='''
#ifdef __cplusplus
}
#endif

#endif // __GENERATOR_AUTOLIB_H__
'''
    with open(autolib_h_file, 'a+') as f:
        f.write(autolib_h_file_content)

    autolib_c_file=str(generatorprotopath)+'/'+'generator_autolib.c'
    autolib_c_file_content='''
};\n
'''
    with open(autolib_c_file, 'a+') as f:
        f.write(autolib_c_file_content)

if __name__ == '__main__':
    program_name, *argv=sys.argv

    if len(argv) < 3:
        print(f'Usage: {program_name} jsonpath generatorprotopath generatorcpath')
        print('ERROR: no args is provided')
        sys.exit(1)

    jsonpath, generatorprotopath, generatorcpath, *argv=argv
    jsonpath, generatorprotopath=util_prepare_paths(jsonpath, generatorprotopath)
    jsonfiles=util_find_jsonfiles(jsonpath)

    Modules_dict={}
    util_generator_autolib_h_c_begin(generatorcpath)
    for j in sorted(jsonfiles):
        # util_print(f'→ {j}', bcolors.BLUE)
        with open(str(j), 'r') as f:
            json_mem=json.load(f, object_pairs_hook=OrderedDict)

        # Module Object
        ModuleName=None
        ModuleName=json_mem.get('Module', {}).get('name', None)
        if util_is_empty(ModuleName):
            util_print(f'[{str(j)}]'+' Module name is invalid', bcolors.RED)
            sys.exit(1)
        ModuleId=None
        ModuleId=json_mem.get('Module', {}).get('id', None)
        if util_is_empty(ModuleId):
            util_print(f'[{str(j)}]'+' Module id is invalid', bcolors.RED)
            sys.exit(1)

        # Check if ModuleName or ModuleId is duplicated
        has_duplicate=False
        for mid, mname in Modules_dict.items():
            if mname == ModuleName:
                util_print(f'[{str(j)}] ERROR: Duplicate ModuleName [{ModuleName}] detected.', bcolors.RED)
                util_print(f'       Current ModuleId: {ModuleId}', bcolors.RED)
                util_print(f'       Conflicts with existing ModuleId: {mid})', bcolors.RED)
                has_duplicate=True
                break
            elif mid == ModuleId:
                util_print(f'[{str(j)}] ERROR: Duplicate ModuleId [{ModuleId}] detected.', bcolors.RED)
                util_print(f'       Current ModuleName: {ModuleName}', bcolors.RED)
                util_print(f'       Conflicts with existing ModuleName: {mname}', bcolors.RED)
                has_duplicate=True
                break

        if has_duplicate:
            sys.exit(1)

        Modules_dict[ModuleId]=ModuleName

        proto_file=util_open_proto_file(str(generatorprotopath), str(ModuleName))
        protooption_file=util_open_protooption_file(str(generatorprotopath), str(ModuleName))
        proto_file.write('enum '+str(ModuleName).upper()+'_SERVICE_ID')
        proto_file.write('\n{')
        if int(ModuleId) == 0: # local_registry
            proto_file.write('\n    option allow_alias = true;')
        proto_file.write('\n    '+'INVALID = 0;')

        StructDef=None
        EnumDef=None
        UnionDef=None
        ProviderService=None

        type_list=['bool', 'string', 'bytes', 'int32', 'uint32', 'int64', 'uint64', 'float', 'double']
        service_list=[]
        struct_list=[]
        enum_list=[]
        union_list=[]
        service_index=0

        service_start_index=(int(ModuleId) << 16) | (0 & 0xFFFF)
        proto_file.write(f'\n    START = 0x{service_start_index:X};')

        # prepare enum/union/struct define
        EnumDef=json_mem.get('EnumDef', {})
        UnionDef=json_mem.get('UnionDef', {})
        StructDef=json_mem.get('StructDef', {})
        ProviderService=json_mem.get('ProviderService', {})

        # enum list
        for key in EnumDef.keys():
            enum_name=key
            if util_is_empty(enum_name):
                util_print(f'enum_name is invalid', bcolors.RED)
                sys.exit(1)
            enum_list.append(enum_name)

        # union list
        for key in UnionDef.keys():
            union_name=key
            if util_is_empty(union_name):
                util_print('union_name is invalid', bcolors.RED)
                sys.exit(1)
            union_list.append(union_name)

        # struct list
        for key in StructDef.keys():
            struct_name=key
            if util_is_empty(struct_name):
                util_print(f'struct_name is invalid', bcolors.RED)
                sys.exit(1)
            struct_list.append(struct_name)

        # ProviderService Object
        if not util_is_empty(ProviderService):
            for i in range(0, len(ProviderService)):
                # type
                ProviderServiceType=None
                ProviderServiceType=ProviderService[i].get('type', None)
                if util_is_empty(ProviderServiceType):
                    util_print(f'{str(ProviderServiceType)} type is invalid', bcolors.RED)
                    sys.exit(1)

                # name
                ProviderServiceName=None
                ProviderServiceName=ProviderService[i].get('name', None)
                if util_is_empty(ProviderServiceName):
                    util_print(f'{str(ProviderServiceName)} name is invalid', bcolors.RED)
                    sys.exit(1)
                service_index+=1
                combined_id=(int(ModuleId) << 16) | (service_index & 0xFFFF)
                proto_file.write('\n    '+str(ProviderServiceType.upper())+'_'+str(ProviderServiceName.upper())+f' = 0x{combined_id:X};')

                # in
                ProviderServiceIn=None
                ProviderServiceIn=ProviderService[i].get('in', None)
                # out
                ProviderServiceOut=None
                ProviderServiceOut=ProviderService[i].get('out', None)
                service_list.append([ProviderServiceType, ProviderServiceName, ProviderServiceIn, ProviderServiceOut])
        service_end_index=(int(ModuleId) << 16) | (0xFFFF & 0xFFFF)
        proto_file.write(f'\n    END = 0x{service_end_index:X};')
        proto_file.write('\n}\n\n')

        # EnumDef
        for key in EnumDef.keys():
            enum_name=key
            proto_file.write('enum '+str(enum_name))
            proto_file.write('\n{')
            enum_member=EnumDef[enum_name]
            for member in enum_member.keys():
                member_dex=enum_member[member]
                proto_file.write('\n    '+str(member)+' = '+str(member_dex)+';')
            proto_file.write('\n}\n\n')

        # UnionDef
        for key in UnionDef.keys():
            union_name=key
            proto_file.write(f'message {union_name}\n{{\n')
            proto_file.write('    oneof union {\n')
            union_members=UnionDef[union_name]
            member_index=1
            for member, member_type_desc in union_members.items():
                if isinstance(member_type_desc, str):
                    member_type=member_type_desc
                    array_size=None
                elif isinstance(member_type_desc, dict) and 'array' in member_type_desc:
                    inner=member_type_desc['array']
                    member_type=next(iter(inner))
                    array_size=inner[member_type]
                else:
                    util_print(f'Invalid union_name member type in {union_name}.{member}', bcolors.RED)
                    sys.exit(1)

                if member_type not in enum_list and member_type not in struct_list and member_type not in union_list and member_type not in type_list:
                    util_print(f'{str(struct_name)} {str(member)} type[{member_array_type}] is invalid', bcolors.RED)
                    sys.exit(1)
                proto_file.write(f'    {member_type} {member} = {member_index};\n')

                if member_type in ['string', 'bytes'] and array_size is not None:
                    option_line=f'{union_name}.{member} max_size:{array_size}\n'
                    protooption_file.write(option_line)

                member_index += 1
            proto_file.write('    }\n')
            proto_file.write('}\n\n')

        # StructDef
        for key in StructDef.keys():
            struct_name=key
            if util_is_empty(struct_name):
                util_print(f'struct_name is invalid', bcolors.RED)
                sys.exit(1)

            proto_file.write('message '+str(struct_name))
            proto_file.write('\n{\n')
            params=[]
            array_num=None
            struct_member=StructDef[key]
            for member in struct_member.keys():
                member_type=struct_member[member]
                member_array_type=util_get_array_type(member_type)
                if member_array_type is not None:
                    member_array=member_type['array']
                    array_num=member_array[member_array_type]
                    if member_array_type == 'bytes' or member_array_type == 'string':
                        if util_is_empty(array_num):
                            util_print(f'{str(struct_name)} {str(member)} array_num is invalid', bcolors.RED)
                            sys.exit(1)
                else:
                    member_array_type=member_type
                    array_num=None
                if member_array_type not in enum_list and member_array_type not in struct_list and member_array_type not in union_list and member_array_type not in type_list:
                    util_print(f'{str(struct_name)} {str(member)} type[{member_array_type}] is invalid', bcolors.RED)
                    sys.exit(1)
                params.append([str(member_array_type),str(member),array_num])
            util_file_process(proto_file, protooption_file, params, struct_name)
            proto_file.write('}\n\n')
        proto_file.close()
        protooption_file.close()
        util_print(f'generating code from {j}', bcolors.BLUE)

        # generator ModuleName_autolib.c
        proto_autolib_file_name=str(generatorcpath)+'/'+str(ModuleName)+'_module_autolib.c'
        proto_autolib_file=open(proto_autolib_file_name, 'w+')
        proto_autolib_file.write('#include \"'+str(ModuleName)+'_module.pb.h'+'\"\n')
        proto_autolib_file.write('#include \"generator_autolib.h\"\n\n')
        proto_autolib_file.write('st_autolib_servicemap_item gst_'+str(ModuleName).lower()+'_autolib_serviceitems[] =')
        proto_autolib_file.write('\n{\n')
        for i in range(0, len(service_list)):
            service_id_name=str(ModuleName).upper()+'_SERVICE_ID_'+str(service_list[i][0]).upper()+'_'+str(service_list[i][1]).upper()
            proto_autolib_file.write('    {'+str(service_id_name)+', ')
            if service_list[i][2] is None:
                proto_autolib_file.write('NULL, ')
                proto_autolib_file.write(str(0) + ', ')
            else:
                proto_autolib_file.write(str(service_list[i][2])+'_fields, ')
                proto_autolib_file.write('sizeof('+str(service_list[i][2])+'), ')

            if service_list[i][3] is None:
                proto_autolib_file.write('NULL, ')
                proto_autolib_file.write(str(0) + '},\n')
            else:
                proto_autolib_file.write(str(service_list[i][3])+'_fields, ')
                proto_autolib_file.write('sizeof('+str(service_list[i][3])+')},\n')
        proto_autolib_file.write('};\n\n')

        proto_autolib_file.write('st_autolib_servicemap gst_'+str(ModuleName).lower()+'_autolib_servicemap =')
        proto_autolib_file.write('\n{\n')
        proto_autolib_file.write('    .items = gst_'+str(ModuleName).lower()+'_autolib_serviceitems, \n')
        proto_autolib_file.write('    .items_size = '+str(len(service_list))+', \n')
        proto_autolib_file.write('};\n')
        proto_autolib_file.close()

        # generator autolib.h/.c
        util_generator_autolib_h_c_handle(generatorcpath, ModuleName, service_list, int(ModuleId))
    util_generator_autolib_h_c_end(generatorcpath)
