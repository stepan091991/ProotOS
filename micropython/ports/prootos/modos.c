/* ports/prootos/modos.c
 * Минимальный os-модуль на базе сисколов ProotOS.
 */

#include "py/runtime.h"
#include "py/objstr.h"
#include "py/objlist.h"
#include "py/mphal.h"
#include "py/mperrno.h"

extern int sys_stat(const char* path);
extern int sys_readdir(const char* path, char* buf, unsigned int bufsize);
extern int sys_unlink(const char* path);
extern int sys_mkdir(const char* path);

static mp_obj_t mod_os_listdir(mp_obj_t path_in) {
    const char* path = mp_obj_str_get_str(path_in);
    static char buf[1024];
    int n = sys_readdir(path, buf, sizeof(buf));
    if (n < 0) mp_raise_OSError(MP_ENOENT);

    mp_obj_t list = mp_obj_new_list(0, NULL);
    char* p = buf;
    for (int i = 0; i < n; i++) {
        unsigned int l = 0;
        while (p[l]) l++;
        mp_obj_list_append(list, mp_obj_new_str(p, l));
        p += l + 1;
    }
    return list;
}
MP_DEFINE_CONST_FUN_OBJ_1(mod_os_listdir_obj, mod_os_listdir);

static mp_obj_t mod_os_stat(mp_obj_t path_in) {
    const char* path = mp_obj_str_get_str(path_in);
    int t = sys_stat(path);
    if (t == 0) mp_raise_OSError(MP_ENOENT);

    /* (mode, ino, dev, nlink, uid, gid, size, atime, mtime, ctime) */
    mp_obj_t tuple[10];
    tuple[0] = mp_obj_new_int(t == 2 ? 0x4000 : 0x8000);
    tuple[1] = mp_obj_new_int(0);
    tuple[2] = mp_obj_new_int(0);
    tuple[3] = mp_obj_new_int(1);
    tuple[4] = mp_obj_new_int(0);
    tuple[5] = mp_obj_new_int(0);
    tuple[6] = mp_obj_new_int(0);
    tuple[7] = mp_obj_new_int(0);
    tuple[8] = mp_obj_new_int(0);
    tuple[9] = mp_obj_new_int(0);
    return mp_obj_new_tuple(10, tuple);
}
MP_DEFINE_CONST_FUN_OBJ_1(mod_os_stat_obj, mod_os_stat);

static mp_obj_t mod_os_remove(mp_obj_t path_in) {
    const char* path = mp_obj_str_get_str(path_in);
    if (sys_unlink(path) < 0) mp_raise_OSError(MP_ENOENT);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mod_os_remove_obj, mod_os_remove);

static mp_obj_t mod_os_mkdir(mp_obj_t path_in) {
    const char* path = mp_obj_str_get_str(path_in);
    if (sys_mkdir(path) < 0) mp_raise_OSError(MP_EEXIST);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mod_os_mkdir_obj, mod_os_mkdir);

static const mp_rom_map_elem_t mod_os_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_uos) },
    { MP_ROM_QSTR(MP_QSTR_listdir), MP_ROM_PTR(&mod_os_listdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_ilistdir), MP_ROM_PTR(&mod_os_listdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_stat),    MP_ROM_PTR(&mod_os_stat_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove),  MP_ROM_PTR(&mod_os_remove_obj) },
    { MP_ROM_QSTR(MP_QSTR_unlink),  MP_ROM_PTR(&mod_os_remove_obj) },
    { MP_ROM_QSTR(MP_QSTR_mkdir),   MP_ROM_PTR(&mod_os_mkdir_obj) },
};
static MP_DEFINE_CONST_DICT(mod_os_globals, mod_os_globals_table);

const mp_obj_module_t mp_module_uos = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mod_os_globals,
};

MP_REGISTER_MODULE(MP_QSTR_uos, mp_module_uos);
MP_REGISTER_MODULE(MP_QSTR_os,  mp_module_uos);
