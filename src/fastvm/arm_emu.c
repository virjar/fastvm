﻿
#include "mcore/mcore.h"
#include "vm.h"
#include "arm_emu.h"
#include <math.h>


#define ARM_REG_R0      0
#define ARM_REG_R1      1
#define ARM_REG_R2      2
#define ARM_REG_R3      3
#define ARM_REG_R4      4
#define ARM_REG_R5      5
#define ARM_REG_R6      6
#define ARM_REG_R7      7
#define ARM_REG_R8      8
#define ARM_REG_R9      9
#define ARM_REG_R10     10
#define ARM_REG_R11     11
#define ARM_REG_R12     12
#define ARM_REG_R13     13
#define ARM_REG_R14     14
#define ARM_REG_R15     15

#define ARM_REG_PC      ARM_REG_R15
#define ARM_REG_LR      ARM_REG_R14
#define ARM_REG_SP      ARM_REG_R13

typedef int         reg_t;

#define BITS_GET(a,offset,len)   ((a >> (offset )) & ((1 << len) - 1))
#define BITS_GET_SH(a, offset, len, sh)      (BITS_GET(a, offset, len) << sh)

struct arm_inst_context {
    reg_t   ld;
    reg_t   lm;
    reg_t   ln;
    int     register_list;
    int     imm;
    int     m;
    int     setflags;
};

struct arm_cpsr {
    unsigned m:  5;
    unsigned t : 1;
    unsigned f : 1;
    unsigned i : 1;
    unsigned a : 1;
    unsigned e : 1;
    unsigned it1 : 6;
    unsigned ge : 4;
    unsigned reserved : 4;
    unsigned j : 1;
    unsigned it2 : 2;
    unsigned q : 1;
    unsigned v : 1;
    unsigned c : 1;
    unsigned z : 1;
    unsigned n : 1;
};

struct arm_emu {
    struct {
        unsigned char*  data;
        int             len;
        int             pos;

        struct arm_inst_context ctx;
    } code;

    char inst_fmt[64];

    int(*inst_func)(unsigned char *inst, int len,  char *inst_str, void *user_ctx);
    void *user_ctx;

    /* emulate stack */
    struct {
        unsigned char*    data;
        int                top;
        int                size;
    } stack;

    struct {
        int                counts;
    } inst;

    int             dump_dfa;
    int             dump_enfa;
    int             dump_inst;

    int             in_it_block;
    int             baseaddr;
    int             thumb;
};

static const char *regstr[] = {
    "r0",
    "r1",
    "r2",
    "r3",
    "r4",
    "r5",
    "r6",
    "r7",
    "r8",
    "r9",
    "r10",
    "r11",
    "r12",
    "r13",
    "lr",
    "pc",
};

typedef int(*arm_inst_func)    (struct arm_emu *emu, uint16_t *inst, int inst_len);

#include "arm_op.h"

static char* reglist2str(int reglist, char *buf)
{
    int i, start_reg = -1, len;
    buf[0] = 0;

    strcat(buf, "{");
    for (i = 0; i < 16; ) {
        if (reglist & (1 << i)) {
            strcat(buf, regstr[i]);

            start_reg = i++;
            while (reglist & (1 << i)) i++;

            if (i != (start_reg + 1)) {
                strcat(buf, "-");
                strcat(buf, regstr[i - 1]);
            }

            strcat(buf, ",");
        }
        else
            i++;
    }
    len = strlen(buf);
    if (buf[len - 1] == ',')  buf[len - 1] = 0;
    strcat(buf, "}");

    return buf;
}

static void arm_prepare_dump(struct arm_emu *emu, const char *fmt, ...)
{
    if (!emu->dump_inst)
        return;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(emu->inst_fmt, sizeof (emu->inst_fmt), fmt, ap);
    va_end(ap);
}

static void arm_dump_inst(struct arm_emu *emu, uint16_t *inst, int inst_len)
{
    int i, len;
    char *buf, *param;
    unsigned char *code = (char *)inst;

    if (!emu->dump_inst)
        return;

    buf = emu->inst_fmt;
    len = strlen(buf);
    for (i = 0; i < len; i++) {
        buf[i] = toupper(buf[i]);
    }

    param = strchr(buf, ' ');
    if (param) {
        *param++ = 0;
        while (isblank(*param)) param++;
    }

    printf("%08x ", emu->baseaddr + emu->code.pos);

    for (i = 0; i < (inst_len * 2); i++) {
        printf("%02x ", code[i]);
    }

    for (; i < 8; i++) {
        printf("   ");
    }

    printf("%-16s %s\n", buf, param);

    emu->inst_fmt[0] = 0;
}

static int t1_inst_lsl(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_lsr(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_asr(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_push(struct arm_emu *emu, uint16_t *code, int len)
{
    char buf[32];

    arm_prepare_dump(emu, "push %s", reglist2str(emu->code.ctx.register_list, buf));

    return 0;
}

static int t1_inst_pop(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t2_inst_push(struct arm_emu *emu, uint16_t *code, int len)
{
    char buf[32];

    arm_prepare_dump(emu, "push.w %s", reglist2str(emu->code.ctx.register_list, buf));
    return 0;
}

static int t1_inst_add(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_add1(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_add2(struct arm_emu *emu, uint16_t *code, int len)
{
    arm_prepare_dump(emu, "add %s, sp, #0x%x", regstr[emu->code.ctx.ld], emu->code.ctx.imm * 4);

    return 0;
}

static int t1_inst_add3(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_add4(struct arm_emu *emu, uint16_t *code, int len)
{
    arm_prepare_dump(emu, "add %s, %s", regstr[emu->code.ctx.ld], regstr[emu->code.ctx.lm]);
    return 0;
}

static int t1_inst_sub(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_sub1(struct arm_emu *emu, uint16_t *code, int len)
{
    arm_prepare_dump(emu, "sub sp, #0x%x", emu->code.ctx.imm * 4);
    return 0;
}

static int t1_inst_mov(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_mov_0100(struct arm_emu *emu, uint16_t *code, int len)
{
    arm_prepare_dump(emu, "mov %s, %s", regstr[emu->code.ctx.ld], regstr[emu->code.ctx.lm]);
    return 0;
}

static int t1_inst_mov1(struct arm_emu *emu, uint16_t *code, int len)
{
    arm_prepare_dump(emu, "mov%c %s, #0x%x", emu->in_it_block ? 'c':'s', regstr[emu->code.ctx.ld], emu->code.ctx.imm);
    return 0;
}

static int t1_inst_cmp(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_cmp_0100(struct arm_emu *emu, uint16_t *code, int len)
{
    arm_prepare_dump(emu, "cmp %s, %s", regstr[emu->code.ctx.lm], regstr[emu->code.ctx.ld]);
    return 0;
}

static int t1_inst_and(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_eor(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_adc(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_sbc(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_ror(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_tst(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_neg(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_cmn(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_orr(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_mul(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_bic(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_mvn(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_cpy(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_ldr1(struct arm_emu *emu, uint16_t *code, int len)
{
    arm_prepare_dump(emu, "ldr %s, [pc, #0x%x] ", regstr[emu->code.ctx.ld], emu->code.ctx.imm * 4);
    return 0;
}

static int t1_inst_ldr2(struct arm_emu *emu, uint16_t *code, int len)
{
    if (emu->code.ctx.imm)
        arm_prepare_dump(emu, "ldr %s, [%s, #0x%x] ", regstr[emu->code.ctx.ld], regstr[emu->code.ctx.lm], emu->code.ctx.imm * 4);
    else
        arm_prepare_dump(emu, "ldr %s, [%s] ", regstr[emu->code.ctx.ld], regstr[emu->code.ctx.lm]);

    return 0;
}

static int t1_inst_str_01101(struct arm_emu *emu, uint16_t *code, int len)
{
    return 0;
}

static int t1_inst_str_10010(struct arm_emu *emu, uint16_t *code, int len)
{
    if (emu->code.ctx.imm)
        arm_prepare_dump(emu, "str %s, [sp,#0x%x]", regstr[emu->code.ctx.ld], emu->code.ctx.imm * 4);
    else
        arm_prepare_dump(emu, "str %s, [sp]");

    return 0;
}

static int t1_inst_ldr_10011(struct arm_emu *emu, uint16_t *code, int len)
{
    if (emu->code.ctx.imm)
        arm_prepare_dump(emu, "ldr %s, [sp, #0x%x]", regstr[emu->code.ctx.ld], emu->code.ctx.imm * 4);
    else
        arm_prepare_dump(emu, "ldr %s, [sp]");
    return 0;
}

static int t1_inst_movt(struct arm_emu *emu, uint16_t *inst, int inst_len)
{
    int imm = BITS_GET_SH(inst[0], 10, 1, 11) + BITS_GET_SH(inst[0], 0, 4, 12) + BITS_GET_SH(inst[1], 12, 3, 8) + BITS_GET_SH(inst[1], 0, 8, 0);

    arm_prepare_dump(emu, "movt %s, #0x%x", regstr[emu->code.ctx.ld], imm);

    return 0;
}

static int t1_inst_movw(struct arm_emu *emu, uint16_t *inst, int inst_len)
{
    int imm = BITS_GET_SH(inst[0], 10, 1, 11) + BITS_GET_SH(inst[0], 0, 4, 12) + BITS_GET_SH(inst[1], 12, 3, 8) + BITS_GET_SH(inst[1], 0, 8, 0);

    arm_prepare_dump(emu, "movw %s, #0x%x", regstr[emu->code.ctx.ld], imm);

    return 0;
}

static int t1_inst_mov_w(struct arm_emu *emu, uint16_t *inst, int inst_len)
{
    int imm = BITS_GET_SH(inst[0], 10, 1, 11) + BITS_GET_SH(inst[1], 12, 3, 8) + BITS_GET_SH(inst[1], 0, 8, 0);

    arm_prepare_dump(emu, "mov.w %s, #0x%x", regstr[emu->code.ctx.ld], ThumbExpandImmWithC(emu, imm));

    return 0;
}

static int t1_inst_bx_0100(struct arm_emu *emu, uint16_t *code, int len)
{
    arm_prepare_dump(emu, "bx %s", regstr[emu->code.ctx.lm]);
    return 0;
}

static int t1_inst_blx_0100(struct arm_emu *emu, uint16_t *code, int len)
{
    arm_prepare_dump(emu, "blx %s", regstr[emu->code.ctx.lm]);

    return 0;
}

static int t1_inst_b(struct arm_emu *emu, uint16_t *code, int len)
{
    arm_prepare_dump(emu, "b 0x%x", emu->baseaddr + emu->code.pos + 4 + SignExtend(emu->code.ctx.imm, 11) * 2);

    return 0;
}

/*

definition about inst reg rule
o[num]:      operate 
i[num]:      immediate
m[num]:      link register
rl[num]:     register list
lm[num]:     src register
hm[num]:     high register
*/
struct arm_inst_desc {
    const char *regexp;
    arm_inst_func    funclist[4];
    const char        *desc[4];
} desclist[]= {
    {"0000    o1    i5  lm3 ld3",           {t1_inst_lsl, t1_inst_lsr}, {"lsl", "lsr"}},
    {"0001    0     i5  lm3 ld3",           {t1_inst_asr}, {"asr"}},
    {"0001    10 o1 lm3 ln3 ld3",           {t1_inst_add, t1_inst_sub}, {"add", "sub"}},
    {"0001    11 o1 i3  ln3 ld3",           {t1_inst_add, t1_inst_sub}, {"add", "sub"}},
    {"0010    o1 ld3    i8",                {t1_inst_mov1, t1_inst_cmp}, {"mov", "cmp"}},
    {"0011    o1 ld3    i8",                {t1_inst_add, t1_inst_sub}, {"add", "sub"}},
    {"0100    0000 o2 lm3 ld3",             {t1_inst_and, t1_inst_eor, t1_inst_lsl, t1_inst_lsr}, {"and", "eor", "lsl2", "lsr2"}},
    {"0100    0001 o2 lm3 ld3",             {t1_inst_asr, t1_inst_adc, t1_inst_sbc, t1_inst_ror}, {"asr", "adc", "sbc", "ror"}},
    {"0100    0010 o2 lm3 ld3",             {t1_inst_tst, t1_inst_neg, t1_inst_cmp_0100, t1_inst_cmn}, {"tst", "neg", "cmp", "cmn"}},
    {"0100    0011 o2 lm3 ld3",             {t1_inst_orr, t1_inst_mul, t1_inst_bic, t1_inst_mvn}, {"orr", "mul", "bic", "mvn"}},
    {"0100    0110 00 lm3 ld3",             {t1_inst_mov_0100}, {"mov"}},
    {"0100    01 o1 0 01 hm3 ld3",          {t1_inst_add4, t1_inst_mov}, {"add", "mov"}},
    {"0100    01 o1 0 10 lm3 hd3",          {t1_inst_add, t1_inst_mov_0100}, {"add", "mov"}},
    {"0100    01 o1 0 11 hm3 hd3",          {t1_inst_add, t1_inst_mov}, {"add", "mov"}},
    {"0100    1 ld3 i8",                    {t1_inst_ldr1}, {"ldr"}},
    {"0100    0111 o1 lm4 000",             {t1_inst_bx_0100, t1_inst_blx_0100}, {"bx", "blx"}},
    {"0110    o1 i5 lm3 ld3",               {t1_inst_str_01101, t1_inst_ldr2},      {"str", "ldr"}},
    {"1001    o1 ld3 i8",                   {t1_inst_str_10010, t1_inst_ldr_10011}, {"str", "ldr"}},
    {"1010    o1 ld3 i8",                   {t1_inst_add1, t1_inst_add2}, {"add", "add"}},
    {"1011    o1 10 m1 rl8",                {t1_inst_push, t1_inst_pop}, {"push", "pop"}},
    {"1011    0000 o1 i7",                  {t1_inst_add3, t1_inst_sub1}, {"add", "sub"}},
    {"1110    0 i11",                       {t1_inst_b}, {"b"}},
    {"1110 1001 0010 1101 0 m1 0 rl13",     {t2_inst_push}, "push.w"},
    {"1111 0i1 00010 s1 11110 i3 ld4 i8",   {t1_inst_mov_w}, "mov.w"},
    {"1111 0i1 101100 i4 0 i3 ld4 i8",      {t1_inst_movt}, "movt"},
    {"1111 0i1 100100 i4 0 i3 ld4 i8",      {t1_inst_movw}, "movw"},
};

static int init_inst_map = 0;

struct inst_node {
    int id;
    struct inst_node *parent;
    struct inst_node *childs[3];

    arm_inst_func func;
    struct dynarray set;
    char *exp;
    char *desc;
};

struct inst_tree {
    int counts;
    struct dynarray arr;
    struct inst_node root;
};

struct arm_inst_engine {
    struct inst_tree    enfa;
    struct inst_tree    dfa;

    int width;
    int height;
    int *trans2d;
};

struct inst_node *inst_node_new(struct inst_tree *tree, struct inst_node *parent)
{
    struct inst_node *node = calloc(1, sizeof (node[0]));

    if (!node)
        vm_error("inst_node_new() failed calloc");

    node->parent = parent;
    node->id = ++tree->counts;
    dynarray_add(&tree->arr, node);

    return node;
}

void inst_node_copy(struct inst_node *dst, struct inst_node *src)
{
    dst->func = src->func;
    dst->exp = strdup(src->exp);
    dst->desc = strdup(src->desc);
}

int inst_node_height(struct inst_node *node)
{
    int i = 0;
    while (node->parent) {
        i++;
        node = node->parent;
    }

    return i;
}

void    inst_node_delete(struct inst_node *root)
{
    int i;
    if (!root)
        return;

    for (i = 0; i < count_of_array(root->childs); i++) {
        inst_node_delete(root->childs[i]);
    }

    if (root->exp)    free(root->exp);
    if (root->desc) free(root->desc);

    free(root);
}

void inst_node__dump_dot(FILE *fp, struct inst_node *root)
{
    int i;

    for (i = 0; i < count_of_array(root->childs); i++) {
        if (!root->childs[i])
            continue;

        fprintf(fp, "%d -> %d [label = \"%d\"]\n", root->id, root->childs[i]->id, i);
        inst_node__dump_dot(fp, root->childs[i]);
    }

    if (root->func) {
        fprintf(fp, "%d [shape=Msquare, label=\"%s\"];\n", root->id, root->desc);
    }
}

void inst_node_dump_dot(const char *filename, struct inst_node *root)
{
    char buf[128];
    FILE *fp = fopen(filename, "w");

    fprintf(fp, "digraph G {\n");

    inst_node__dump_dot(fp, root);

    fprintf(fp, "%d [shape=Mdiamond];\n", root->id);

    fprintf(fp, "}\n");

    fclose(fp);

    sprintf(buf, "dot -Tpng %s -o %s.png", filename, filename);
    system(buf);
}

struct arm_inst_engine *inst_engine_create()
{
    struct arm_inst_engine *engine = NULL;

    engine = calloc(1, sizeof(engine[0]));
    if (!engine)
        vm_error("failed calloc()");

    return engine;
}

int arm_insteng_add_exp(struct arm_inst_engine *en, const char *exp, const char *desc, arm_inst_func func)
{
    struct inst_node *root = &en->enfa.root;
    int j, len = strlen(exp), rep, height, idx;
    const char *s = exp;

    for (; *s; s++) {
        if (isblank(*s))    continue;
        switch (*s) {
        case '0':
        case '1':
            idx = *s - '0';
            if (!root->childs[idx]) {
                root->childs[idx] = inst_node_new(&en->enfa, root);
            }

            root = root->childs[idx];
            break;

            /* setflags */
        case 's':
            /* immediate */
        case 'i':

            /* more register，一般是对寄存器列表的补充 */
        case 'm':
loop_label:
            rep = atoi(&s[1]);
            if (!rep) 
                goto fail_label;

            for (j = 0; j < rep; j++) {
                if (!root->childs[2])
                    root->childs[2] = inst_node_new(&en->enfa, root);
                root = root->childs[2];
            }

            while (s[1] && isdigit(s[1])) s++;
            break;

        case 'r':
            if (s[1] != 'l')
                goto fail_label;

            s++;
            goto loop_label;

            /* register */
        case 'l':
            if (s[1] != 'm' && s[1] != 'd' && s[1] != 'n')
                goto fail_label;

            s++;
            goto loop_label;

        case 'h':
            if (s[1] != 'm' && s[1] != 'd' && s[1] != 'n')
                goto fail_label;

            s++;
            goto loop_label;

    fail_label:
        default:
            vm_error("inst expression [%s], position[%d:%s]\n", exp, (s - exp), s);
            break;
        }
    }

    root->exp = strdup(exp);
    root->func = func;
    root->desc = strdup(desc);

    // FIXME:测试完毕可以关闭掉，验证层数是否正确
    height = inst_node_height(root);
    if (height != 16 && height != 32) {
        vm_error("inst express error[%s], not size 16 or 32\n", exp);
    }

    return 0;
}

static struct arm_inst_engine *g_eng = NULL;

static struct inst_node *inst_node_find(struct inst_tree *tree, struct dynarray *arr)
{
    struct inst_node *node;
    int i;
    for (i = 0; i < tree->arr.len; i++) {
        node = (struct inst_node *)tree->arr.ptab[i];
        if (0 == dynarray_cmp(arr, &node->set))
            return node;
    }

    return NULL;
}

static int arm_insteng_gen_dfa(struct arm_inst_engine *eng)
{
    struct inst_node *nfa_root = &eng->enfa.root;

    struct inst_node *dfa_root = &eng->dfa.root;
    struct inst_node *node_stack[512], *droot, *nroot, *dnode1, *nnode;
    struct dynarray set = {0};
    int stack_top = -1, i, j, need_split;

#define istack_push(a)            (node_stack[++stack_top] = a)
#define istack_pop()            node_stack[stack_top--]        
#define istack_is_empty()        (stack_top == -1)

    dynarray_add(&dfa_root->set, nfa_root);
    istack_push(dfa_root);
    dynarray_reset(&set);

    while (!istack_is_empty()) {
        droot = istack_pop();

        for (i = need_split = 0; i < droot->set.len; i++) {
            nroot = (struct inst_node *)droot->set.ptab[i];
            if (nroot->childs[0] || nroot->childs[1]) {
                need_split = 1;
                break;
            }
        }

        for (i = need_split ? 0:2; i < (3 - need_split); i++) {
            dnode1 = NULL;

            for (j = 0; j < droot->set.len; j++) {
                if (!(nroot = (struct inst_node *)droot->set.ptab[j]))
                    continue;

                if (nroot->childs[i])    dynarray_add(&set, nroot->childs[i]);
                if ((i != 2) && nroot->childs[2])    dynarray_add(&set, nroot->childs[2]);
            }

            if (!dynarray_is_empty(&set)) {
                dnode1 = inst_node_find(&eng->dfa, &set);
                if (!dnode1) {
                    dnode1 = inst_node_new(&eng->dfa, droot);
                    dynarray_copy(&dnode1->set, &set);
                    istack_push(dnode1);
                }

                droot->childs[i] = dnode1;
            }

            /* 检查生成的DFA节点中包含的NFA节点，是否有多个终结态，有的话报错，没有的话把NFA的
            状态拷贝到DFA中去 */
            if (dnode1) {
                for (j = 0; j < dnode1->set.len; j++) {
                    if (!(nnode = (struct inst_node *)dnode1->set.ptab[j]) || !nnode->func)    continue;

                    if (dnode1->func) {
                        vm_error("conflict end state\n");
                    }

                    inst_node_copy(dnode1, nnode);
                }
            }

            dynarray_reset(&set);
        }
    }

    return 0;
}

static int arm_insteng_gen_trans2d(struct arm_inst_engine *eng)
{
    int i;
    struct inst_tree *tree = &eng->dfa;
    struct inst_node *node;
    eng->width = eng->dfa.counts;
    eng->height = 2;

    eng->trans2d = calloc(1, eng->width * eng->height * sizeof (eng->trans2d[0]));
    if (!eng->trans2d)
        vm_error("trans2d calloc failure");

    for (i = 0; i < tree->arr.len; i++) {
        node = (struct inst_node *)tree->arr.ptab[i];

        if (node->childs[2]) {
            eng->trans2d[eng->width + node->id] = node->childs[2]->id;
            eng->trans2d[node->id] = node->childs[2]->id;
            continue;
        }

        if (node->childs[0]) {
            eng->trans2d[node->id] = node->childs[0]->id;
        }

        if (node->childs[1]) {
            eng->trans2d[eng->width + node->id] = node->childs[1]->id;
        }
    }

    return 0;
}

static int arm_insteng_init(struct arm_emu *emu)
{
    int i, j, k, m, n, len;
    const char *exp;
    char buf[128];

    if (g_eng)
        return 0;

    g_eng = calloc(1, sizeof (g_eng[0]));
    if (!g_eng)
        vm_error("arm_insteng_init() calloc failure");

    dynarray_add(&g_eng->enfa.arr, &g_eng->enfa.root);
    dynarray_add(&g_eng->dfa.arr, &g_eng->dfa.root);

    for (i = 0; i < count_of_array(desclist); i++) {
        exp = desclist[i].regexp;
        len = strlen(exp);
        for (j = k = 0; j < len; k++, j++) {
            if (exp[j] == 'o') {
                int bits = atoi(exp + j + 1);
                if (!bits)
                    vm_error("express error, unknown o token, %s\n", exp);

                while (isdigit(exp[++j]));

                int pow1 = (int)pow(2, bits);
                for (n = 0; n < pow1; n++) {
                    for (m = 0; m < bits; m++) {
                        buf[k + m] = !!(n & (1 <<  (bits - m - 1))) + '0';
                    }

                    strcpy(buf + k + bits, exp + j);
                    arm_insteng_add_exp(g_eng, buf, desclist[i].desc[n], desclist[i].funclist[n]);
                }
                break;
            }
            else {
                buf[k] = exp[j];
            }
        }
        buf[k] = 0;

        if (j == len)
            arm_insteng_add_exp(g_eng, buf, desclist[i].desc[0], desclist[i].funclist[0]);
    }


    arm_insteng_gen_dfa(g_eng);

    if (emu->dump_enfa)
        inst_node_dump_dot("enfa.dot", &g_eng->enfa.root);

    if (emu->dump_dfa)
        inst_node_dump_dot("dfa.dot", &g_eng->dfa.root);

    arm_insteng_gen_trans2d(g_eng);

    init_inst_map = 1;

    return 0;
}

static int arm_insteng_uninit()
{
}

static int arm_insteng_retrieve_context(struct arm_emu *emu, struct inst_node *inst_node, uint8_t *code, int code_len);

static void arm_inst_contenxt_init(struct arm_inst_context *ctx)
{
    memset(ctx, 0, sizeof (ctx[0]));
    ctx->ld = -1;
    ctx->lm = -1;
    ctx->ln = -1;
}

static int arm_insteng_decode(struct arm_emu *emu, uint8_t *code, int len)
{
    struct inst_node *node = NULL;
    int i, j, from = 0, to, bit;
    if (!g_eng) {
        arm_insteng_init(emu);
    }

    /* arm 解码时，假如第一个16bit没有解码到对应的thumb指令终结态，则认为是thumb32，开始进入
    第2轮的解码 */

    //printf("start state:%d\n", from);
    for (i = from = 0; i < 2; i++) {
        uint16_t inst = ((uint16_t *)code)[i];
        for (j = 0; j < 16; j++) {
            bit = !!(inst & (1 << (15 - j)));
            to = g_eng->trans2d[g_eng->width * bit + from];

            if (!to) 
                vm_error("arm_insteng_decode() run on un-support state, code[%02x %02x]\n", code[0], code[1]);

            node = ((struct inst_node *)g_eng->dfa.arr.ptab[to]);
            from = node->id;

            //printf("%d ", from);
        }

        if (node->func)
            break;
    }
    //printf("\n");

    if (!node->func) {
        vm_error("arm_insteng_decode() meet unkown instruction, code[%02x %02x]\n", code[0], code[1]);
    }

    ++i;

    arm_insteng_retrieve_context(emu, node, code, i * 2);
    node->func(emu, (uint16_t *)code, i);
    arm_dump_inst(emu, (uint16_t *)code, i);

    return i * 2;
}


/*

@inst[in]   DFA inst node
@code[in]   binary code ptr
@len[in]    code len
*/
static int arm_insteng_retrieve_context(struct arm_emu *emu, struct inst_node *inst_node, uint8_t *code, int code_len)
{
    int i, len, c;
    char *exp = inst_node->exp;
    struct arm_inst_context *ctx = &emu->code.ctx;

    arm_inst_contenxt_init(ctx);

    uint16_t inst = *(uint16_t *)code;

    i = 0;
    while(*exp) {
        switch ((c = *exp)) {
        case '0': case '1':
            exp++; i++; 
            break;
        case ' ':
            exp++;
            break;

        case 's':
        case 'i':
            len = atoi(++exp);

            if (*exp == 's')
                ctx->setflags = BITS_GET(inst, 16 - i - len, len);
            else
                ctx->imm = BITS_GET(inst, 16 -i - len, len);
            while (isdigit(*exp)) exp++;
            i += len;
            break;

        case 'm':
            len = 1;
            ctx->m = BITS_GET(inst, 16 - i - len, len);
            if (ctx->m) {
                ctx->register_list |= (1 << ARM_REG_LR);
            }
            exp += 2; i += len;
            break;

        case 'h':
        case 'l':
            exp++;
            len = atoi(exp + 1);
            switch (*exp++) {
            case 'n':
                ctx->ln = BITS_GET(inst, 16 - i - len, len) + (c == 'h') * 8;
                break;

            case 'm':
                ctx->lm = BITS_GET(inst, 16 - i - len, len) + (c == 'h') * 8;
                break;

            case 'd':
                ctx->ld = BITS_GET(inst, 16 - i - len, len) + (c == 'h') * 8;
                break;

            default:
                goto fail_label;
            }
            i += len;

            while (isdigit(*exp)) exp++;
            break;

        case 'r':
            exp++;
            if (*exp == 'l') {
                len = atoi(++exp);
                ctx->register_list |= BITS_GET(inst, 16 - i - len, len);
            }
            else
                goto fail_label;
            while (isdigit(*exp)) exp++;
            i += len;
            break;

        fail_label:
        default:
            vm_error("inst exp [%s], un-expect token[%s]\n", inst_node->exp, exp);
            break;
        }

        if ((i == 16) && (code_len == 4)) {
            i = 0;
            inst = *(uint16_t *)(code + 2);
        }
    }

    return 0;
}

struct arm_emu   *arm_emu_create(struct arm_emu_create_param *param)
{
    struct arm_emu *emu;

    emu = calloc(1, sizeof (emu[0]));
    if (!emu) {
    }

    emu->code.data = param->code;
    emu->code.len = param->code_len;
    emu->dump_inst = 1;
    emu->baseaddr = param->baseaddr;

    return emu;
}

void    arm_emu_destroy(struct arm_emu *vm)
{
    free(vm);
}

int        arm_emu_run(struct arm_emu *emu)
{
    unsigned char *code = emu->code.data + emu->code.pos;
    int len = emu->code.len - emu->code.pos;
    int ret;

    ret = arm_insteng_decode(emu, emu->code.data + emu->code.pos, emu->code.len - emu->code.pos);
    if (ret < 0) {
        return -1;
    }
    emu->code.pos += ret;

    return 0;
}

int        arm_emu_run_once(struct arm_emu *vm, unsigned char *code, int code_len)
{
    return 0;
}

void        arm_emu_dump(struct arm_emu *emu)
{
}
