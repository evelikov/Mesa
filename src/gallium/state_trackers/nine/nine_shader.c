#include "nine_shader.h"

#include "pipe/p_shader_tokens.h"

#define DBG_CHANNEL DBG_SHADER

/* version token */
#define VERSION_MINOR(t)    ((t) & 0xFF)
#define VERSION_MAJOR(t)    (((t) >> 8) & 0xFF)
#define VERSION_TYPE(t)     (((t) >> 16) & 0xFFFF)

/* instruction token */
#define INSTRUCTION_OPCODE(t)       ((t) & 0xFFFF)
#define INSTRUCTION_LENGTH(t)       (((t) >> 24) & 0xF)
#define INSTRUCTION_PREDICATED(t)   (((t) >> 28) & 0x1)
#define INSTRUCTION_COISSUE(t)      (((t) >> 30) & 0x1)

#define COMMENT_LENGTH(t)   (((t) >> 16) & 0x7FFF)

/* common to all registers */
#define REG_TYPE(t)     (((((t) >> 11) & 0x3) << 3) | (((t) >> 28) & 0x7))
#define REG_OFFSET(t)   ((t) & 0x7FF)
#define REG_RELATIVE(t) (((t) >> 13) & 0x1) /* VS >= 2.0 and PS >= 3.0 */

/* src registers only */
#define SRC_SWIZZLE_X(t)    (((t) >> 16) & 0x3)
#define SRC_SWIZZLE_Y(t)    (((t) >> 18) & 0x3)
#define SRC_SWIZZLE_Z(t)    (((t) >> 20) & 0x3)
#define SRC_SWIZZLE_W(t)    (((t) >> 22) & 0x3)
#define SRC_MODIFIER(t)     (((t) >> 24) & 0xF)

/* dst registers only */
#define DST_WRITEMASK(t)    (((t) >> 16) & 0xF)
#define DST_MODIFIER(t)     (((t) >> 20) & 0xF)
#define DST_SHIFT(t)        (((t) >> 24) & 0xF)

/* source modifiers (sequential) */
#define SMOD_NONE           0
#define SMOD_NEGATE         1 /* all, not ps: m3x2, m3x3, m3x4, m4x3, m4x4 */
#define SMOD_BIAS           2 /* <= PS 1.4 (x-0.5) */
#define SMOD_BIAS_NEGATE    3 /* <= PS 1.4 (-(x-0.5)) */
#define SMOD_SIGN           4 /* <= PS 1.4 (2(x-0.5)) */
#define SMOD_SIGN_NEGATE    5 /* <= PS 1.4 (-2(x-0.5)) */
#define SMOD_COMPLEMENT     6 /* <= PS 1.4 (1-x) */
#define SMOD_MUL2           7 /* = PS 1.4 (2x) */
#define SMOD_MUL2_NEGATE    8 /* = PS 1.4 (-2x) */
#define SMOD_DIVZ           9 /* <= PS 1.4, tex{ld,crd} (.xy/.z), z=0 => .11 */
#define SMOD_DIVW           10 /* <= PS 1.4, tex{ld,crd} (.xy/.w), w=0 => .11 */
#define SMOD_ABS            11 /* >= SM 3.0 (abs(x)) */
#define SMOD_ABS_NEGATE     12 /* >= SM 3.0 (-abs(x)) */
#define SMOD_NOT            13 /* >= SM 2.0 pedication only */

/* destination modifiers (bitfields) */
#define DMOD_SATURATE   0x1
#define DMOD_PARTIAL    0x2
#define DMOD_CENTROID   0x4

/* d3d9 processors */
#define D3DSHADER_VERTEX    0xFFFE
#define D3DSHADER_FRAGMENT  0xFFFF

struct shader_translator
{
    /* shader version */
    unsigned major;
    unsigned minor;

    /* shader type (TGSI_PROCESSOR_{VERTEX,FRAGMENT}) */
    unsigned processor;

    /* representation stuff */
    struct ureg_program *ureg;
    const DWORD *tokens;
};

/* helpers */
#define UREG        (translator->ureg)
#define TOKEN(i)    (translator->tokens[i])

#define TRANS_VER       ((translator->major << 8) | translator->minor)
#define V(maj, min)     (((maj) << 8) | (min))
#define VMIN(v)         ((v) & 0xFF)
#define VMAJ(v)         (((v) >> 8) & 0xFF)
#define VNOTSUPPORTED   0, 0

#define IS_VERTEX   (translator->processor == TGSI_PROCESSOR_VERTEX)
#define IS_FRAGMENT (translator->processor == TGSI_PROCESSOR_FRAGMENT)

static INLINE enum tgsi_file_type
d3dfile_to_tgsi( struct shader_translator *translator,
                 unsigned file )
{
    switch (file) {
        case D3DSPR_TEMP: return TGSI_FILE_TEMPORARY;
        case D3DSPR_INPUT: return TGSI_FILE_INPUT;
        case D3DSPR_CONST: return TGSI_FILE_CONSTANT;

        /* these are the same */
        case D3DSPR_TEXTURE:
        case D3DSPR_ADDR:
            if (IS_FRAGMENT) {
                return TGSI_FILE_ADDRESS;
            } else {
                return TGSI_FILE_RESOURCE; /* XXX not sure */
            }

        case D3DSPR_RASTOUT:
            if (IS_VERTEX) { return TGSI_FILE_?; }
            break;

        /* these are the same */
        case D3DSPR_TEXCRDOUT:
        case D3DSPR_OUTPUT:
            if (IS_VERTEX) {
                if (SHADER_VER < VER(3, 0)) {
                    return TGSI_FILE_?; /* XXX */
                } else {
                    return TGSI_FILE_OUTPUT;
                }
            }
            break;

        case D3DSPR_CONSTINT: return TGSI_FILE_CONST; /* XXX check */
        case D3DSPR_COLOROUT: return TGSI_FILE_OUTPUT; /* XXX check */
        case D3DSPR_DEPTHOUT: return TGSI_FILE_DEPTHOUT; /* XXX check */
        case D3DSPR_SAMPLER: return TGSI_FILE_SAMPLER;
        case D3DSPR_CONST2: return TGSI_FILE_CONST; /* XXX check */
        case D3DSPR_CONST3: return TGSI_FILE_CONST; /* XXX check */
        case D3DSPR_CONST4: return TGSI_FILE_CONST; /* XXX check */
        case D3DSPR_CONSTBOOL: return TGSI_FILE_CONST; /* XXX check */
        case D3DSPR_LOOP: return TGSI_FILE_?; /* XXX */
        case D3DSPR_TEMPFLOAT16: return TGSI_FILE_?; /* XXX */
        case D3DSPR_MISCTYPE: return TGSI_FILE_?; /* XXX */
        case D3DSPR_LABEL: return TGSI_FILE_?; /* XXX */
        case D3DSPR_PREDICATE: return TGSI_FILE_PREDICATE;

        default:
            return TGSI_FILE_NULL;
    }
}

static INLINE const char *
d3dsio_to_string( unsigned opcode )
{
    static const char *names[] = {
        "NOP",
        "MOV",
        "ADD",
        "SUB",
        "MAD",
        "MUL",
        "RCP",
        "RSQ",
        "DP3",
        "DP4",
        "MIN",
        "MAX",
        "SLT",
        "SGE",
        "EXP",
        "LOG",
        "LIT",
        "DST",
        "LRP",
        "FRC",
        "M4x4",
        "M4x3",
        "M3x4",
        "M3x3",
        "M3x2",
        "CALL",
        "CALLNZ",
        "LOOP",
        "RET",
        "ENDLOOP",
        "LABEL",
        "DCL",
        "POW",
        "CRS",
        "SGN",
        "ABS",
        "NRM",
        "SINCOS",
        "REP",
        "ENDREP",
        "IF",
        "IFC",
        "ELSE",
        "ENDIF",
        "BREAK",
        "BREAKC",
        "MOVA",
        "DEFB",
        "DEFI",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "TEXCOORD",
        "TEXKILL",
        "TEX",
        "TEXBEM",
        "TEXBEML",
        "TEXREG2AR",
        "TEXREG2GB",
        "TEXM3x2PAD",
        "TEXM3x2TEX",
        "TEXM3x3PAD",
        "TEXM3x3TEX",
        NULL,
        "TEXM3x3SPEC",
        "TEXM3x3VSPEC",
        "EXPP",
        "LOGP",
        "CND",
        "DEF",
        "TEXREG2RGB",
        "TEXDP3TEX",
        "TEXM3x2DEPTH",
        "TEXDP3",
        "TEXM3x3",
        "TEXDEPTH",
        "CMP",
        "BEM",
        "DP2ADD",
        "DSX",
        "DSY",
        "TEXLDD",
        "SETP",
        "TEXLDL",
        "BREAKP"
    };

    if (opcode < sizeof(names)/sizeof(*names)) { return names[opcode]; }

    switch (opcode) {
        case D3DSIO_PHASE: return "PHASE";
        case D3DSIO_COMMENT: return "COMMENT";
        case D3DSIO_END: return "END";
        default:
            break;
    }

    return NULL;
}

#define NULL_INSTRUCTION            { 0, { 0, 0 }, { 0, 0 }, 0, 0, NULL }
#define IS_VALID_INSTRUCTION(inst)  ((inst).vert_version.min | \
                                     (inst).vert_version.max | \
                                     (inst).frag_version.min | \
                                     (inst).frag_version.max)

#define SPECIAL(name) \
    NineTranslateInstruction_##name

#define DECL_SPECIAL(name) \
    static HRESULT \
    NineTranslateInstruction_##name( struct shader_translator *translator, \
                                     struct ureg_program *ureg, \
                                     const DWORD *tokens )

/* tokens point to the current position in the token array, and the return
 * value is the amount of DWORDs to skip or if negative, a D3DERR_* */
typedef HRESULT (*translate_instruction_func)( struct shader_translator *,
                                               struct ureg_program *,
                                               const DWORD * );

DECL_SPECIAL(M4x4)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(M4x3)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(M3x4)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(M3x3)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(M3x2)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(CALL)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(CALLNZ)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(LOOP)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(RET)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(ENDLOOP)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(LABEL)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(DEF)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(SGN)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(SINCOS)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(REP)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(ENDREP)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(IF)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(IFC)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(ELSE)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(BREAKC)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(MOVA)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(DEFB)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(DEFI)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXCOORD)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXKILL)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEX)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXBEM)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXBEML)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXREG2AR)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXREG2GB)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXM3x2PAD)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXM3x2TEX)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXM3x3PAD)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXM3x3TEX)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXM3x3SPEC)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXM3x3VSPEC)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(DEF)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXREG2RGB)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXDP3TEX)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXM3x2DEPTH)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXDP3)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXM3x3)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXDEPTH)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(BEM)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXLDD)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(SETP)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(TEXLDL)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(BREAKP)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(PHASE)
{
    return D3DERR_INVALIDCALL;
}

DECL_SPECIAL(COMMENT)
{
    return D3DERR_INVALIDCALL;
}

struct d3d_inst
{
    /* NOTE: 0 is a valid TGSI opcode, but if handler is set, this parameter
     * should be ignored completely */
    unsigned opcode;

    /* Versions are still set even handler is set */
    struct {
        unsigned min;
        unsigned max;
    } vert_version, frag_version;

    /* These are zero'd out if hander is set */
    unsigned ndst;
    unsigned nsrc;

    /* Some instructions don't map perfectly, so use a special handler */
    translate_instruction_func handler;
} inst_table[] = {
    { TGSI_OPCODE_NOP, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 0, 0, NULL },
    { TGSI_OPCODE_MOV, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 1, NULL },
    { TGSI_OPCODE_ADD, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 2, NULL },
    { TGSI_OPCODE_SUB, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 2, NULL },
    { TGSI_OPCODE_MAD, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 3, NULL },
    { TGSI_OPCODE_MUL, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 2, NULL },
    { TGSI_OPCODE_RCP, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 1, NULL },
    { TGSI_OPCODE_RSQ, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 1, NULL },
    { TGSI_OPCODE_DP3, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 2, NULL },
    { TGSI_OPCODE_DP4, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 2, NULL },
    { TGSI_OPCODE_MIN, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 2, NULL },
    { TGSI_OPCODE_MAX, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 2, NULL },
    { TGSI_OPCODE_SLT, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 2, NULL },
    { TGSI_OPCODE_SGE, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 2, NULL },
    { TGSI_OPCODE_EX2, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 1, NULL },
    { TGSI_OPCODE_LG2, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 1, NULL },
    { TGSI_OPCODE_LIT, { V(0,0), V(3,0) }, { VNOTSUPPORTED  }, 1, 1, NULL },
    { TGSI_OPCODE_DST, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 2, NULL },
    { TGSI_OPCODE_LRP, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 3, NULL },
    { TGSI_OPCODE_FRC, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 1, NULL },

    /* Matrix multiplication */
    { 0, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 0, 0, SPECIAL(M4x4) },
    { 0, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 0, 0, SPECIAL(M4x3) },
    { 0, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 0, 0, SPECIAL(M3x4) },
    { 0, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 0, 0, SPECIAL(M3x3) },
    { 0, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 0, 0, SPECIAL(M3x2) },

    /* Functions and loops */
    { 0, { V(2,0), V(3,0) }, { V(2,1), V(3,0) }, 0, 0, SPECIAL(CALL) },
    { 0, { V(2,0), V(3,0) }, { V(2,1), V(3,0) }, 0, 0, SPECIAL(CALLNZ) },
    { 0, { V(3,0), V(3,0) }, { V(3,0), V(3,0) }, 0, 0, SPECIAL(LOOP) },
    { 0, { V(2,0), V(3,0) }, { V(2,1), V(3,0) }, 0, 0, SPECIAL(RET) },
    { 0, { V(3,0), V(3,0) }, { V(3,0), V(3,0) }, 0, 0, SPECIAL(ENDLOOP) },
    { 0, { V(2,0), V(3,0) }, { V(2,1), V(3,0) }, 0, 0, SPECIAL(LABEL) },

    /* Input/output declaration */
    { 0, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 0, 0, SPECIAL(DEF) },

    { TGSI_OPCODE_POW, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 2, NULL },
    { TGSI_OPCODE_XPD, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 2, NULL },
    { 0, { V(2,0), V(3,0) }, { VNOTSUPPORTED  }, 0, 0, SPECIAL(SGN) },
    { TGSI_OPCODE_ABS, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 1, NULL },
    { TGSI_OPCODE_NRM, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 1, NULL },
    { 0, { V(2,0), V(3,0) }, { V(2,0), V(3,0) }, 0, 0, SPECIAL(SINCOS) },

    /* More flow control */
    { 0, { V(2,0), V(3,0) }, { V(2,1), V(3,0) }, 0, 0, SPECIAL(REP) },
    { 0, { V(2,0), V(3,0) }, { V(2,1), V(3,0) }, 0, 0, SPECIAL(ENDREP) },
    { 0, { V(2,0), V(3,0) }, { V(2,1), V(3,0) }, 0, 0, SPECIAL(IF) },
    { 0, { V(2,1), V(3,0) }, { V(2,1), V(3,0) }, 0, 0, SPECIAL(IFC) },
    { 0, { V(2,0), V(3,0) }, { V(2,1), V(3,0) }, 0, 0, SPECIAL(ELSE) },
    { TGSI_OPCODE_ENDIF, { V(2,0), V(3,0) }, { V(2,1), V(3,0) }, 0, 0, NULL },
    { TGSI_OPCODE_BREAK, { V(2,1), V(3,0) }, { V(2,1), V(3,0) }, 0, 0, NULL },
    { 0, { V(2,1), V(3,0) }, { V(2,1), V(3,0) }, 0, 0, SPECIAL(BREAKC) },

    /* Special integer MOV to ADDRESS file */
    { 0, { V(2,0), V(3,0) }, { VNOTSUPPORTED  }, 0, 0, SPECIAL(MOVA) },

    /* Non-float immediates */
    { 0, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 0, 0, SPECIAL(DEFB) },
    { 0, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 0, 0, SPECIAL(DEFI) },

    NULL_INSTRUCTION,
    NULL_INSTRUCTION,
    NULL_INSTRUCTION,
    NULL_INSTRUCTION,
    NULL_INSTRUCTION,
    NULL_INSTRUCTION,
    NULL_INSTRUCTION,
    NULL_INSTRUCTION,
    NULL_INSTRUCTION,
    NULL_INSTRUCTION,
    NULL_INSTRUCTION,
    NULL_INSTRUCTION,
    NULL_INSTRUCTION,
    NULL_INSTRUCTION,
    NULL_INSTRUCTION,

    /* Tex stuff */
    { 0, { VNOTSUPPORTED  }, { V(0,0), V(1,4) }, 0, 0, SPECIAL(TEXCOORD) },
    { 0, { VNOTSUPPORTED  }, { V(0,0), V(3,0) }, 0, 0, SPECIAL(TEXKILL) },
    { 0, { VNOTSUPPORTED  }, { V(0,0), V(3,0) }, 0, 0, SPECIAL(TEX) },
    { 0, { VNOTSUPPORTED  }, { V(0,0), V(1,3) }, 0, 0, SPECIAL(TEXBEM) },
    { 0, { VNOTSUPPORTED  }, { V(0,0), V(1,3) }, 0, 0, SPECIAL(TEXBEML) },
    { 0, { VNOTSUPPORTED  }, { V(0,0), V(1,3) }, 0, 0, SPECIAL(TEXREG2AR) },
    { 0, { VNOTSUPPORTED  }, { V(0,0), V(1,3) }, 0, 0, SPECIAL(TEXREG2GB) },
    { 0, { VNOTSUPPORTED  }, { V(0,0), V(1,3) }, 0, 0, SPECIAL(TEXM3x2PAD) },
    { 0, { VNOTSUPPORTED  }, { V(0,0), V(1,3) }, 0, 0, SPECIAL(TEXM3x2TEX) },
    { 0, { VNOTSUPPORTED  }, { V(0,0), V(1,3) }, 0, 0, SPECIAL(TEXM3x3PAD) },
    { 0, { VNOTSUPPORTED  }, { V(0,0), V(1,3) }, 0, 0, SPECIAL(TEXM3x3TEX) },
    NULL_INSTRUCTION,
    { 0, { VNOTSUPPORTED  }, { V(0,0), V(1,3) }, 0, 0, SPECIAL(TEXM3x3SPEC) },
    { 0, { VNOTSUPPORTED  }, { V(0,0), V(1,3) }, 0, 0, SPECIAL(TEXM3x3VSPEC) },

    { TGSI_OPCODE_EXP, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 1, NULL },
    { TGSI_OPCODE_LOG, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 1, 1, NULL },
    { TGSI_OPCODE_CND, { VNOTSUPPORTED  }, { V(0,0), V(1,4) }, 1, 1, NULL },

    /* Float immediates */
    { 0, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 0, 0, SPECIAL(DEF) },

    /* More tex stuff */
    { 0, { VNOTSUPPORTED  }, { V(1,2), V(1,3) }, 0, 0, SPECIAL(TEXREG2RGB) },
    { 0, { VNOTSUPPORTED  }, { V(1,2), V(1,3) }, 0, 0, SPECIAL(TEXDP3TEX) },
    { 0, { VNOTSUPPORTED  }, { V(1,3), V(1,3) }, 0, 0, SPECIAL(TEXM3x2DEPTH) },
    { 0, { VNOTSUPPORTED  }, { V(1,2), V(1,3) }, 0, 0, SPECIAL(TEXDP3) },
    { 0, { VNOTSUPPORTED  }, { V(1,2), V(1,3) }, 0, 0, SPECIAL(TEXM3x3) },
    { 0, { VNOTSUPPORTED  }, { V(1,4), V(1,4) }, 0, 0, SPECIAL(TEXDEPTH) },

    /* Misc */
    { TGSI_OPCODE_CMP, { VNOTSUPPORTED  }, { V(1,2), V(3,0) }, 1, 3, NULL },
    { 0, { VNOTSUPPORTED  }, { V(1,4), V(1,4) }, 0, 0, SPECIAL(BEM) },
    { TGSI_OPCODE_DP2A, { VNOTSUPPORTED  }, { V(2,0), V(3,0) }, 1, 3, NULL },
    { TGSI_OPCODE_DDX, { VNOTSUPPORTED  }, { V(2,1), V(3,0) }, 1, 1, NULL },
    { TGSI_OPCODE_DDY, { VNOTSUPPORTED  }, { V(2,1), V(3,0) }, 1, 1, NULL },
    { 0, { VNOTSUPPORTED  }, { V(2,1), V(3,0) }, 0, 0, SPECIAL(TEXLDD) },
    { 0, { V(0,0), V(3,0) }, { V(2,1), V(3,0) }, 0, 0, SPECIAL(SETP) },
    { 0, { V(3,0), V(3,0) }, { V(3,0), V(3,0) }, 0, 0, SPECIAL(TEXLDL) },
    { 0, { V(0,0), V(3,0) }, { V(2,1), V(3,0) }, 0, 0, SPECIAL(BREAKP) }
};

struct d3d_inst inst_phase =
    { 0, { VNOTSUPPORTED  }, { V(1,4), V(1,4) }, 0, 0, SPECIAL(PHASE) };

struct d3d_inst inst_comment =
    { 0, { V(0,0), V(3,0) }, { V(0,0), V(3,0) }, 0, 0, SPECIAL(COMMENT) };

static INLINE HRESULT
NineTranslateInstruction_Generic( struct shader_translator *translator,
                                  struct ureg_program *ureg,
                                  const DWORD *tokens,
                                  struct d3d_inst *inst )
{
    return D3DERR_INVALIDCALL;
}

static INLINE HRESULT
NineShaderTranslator_Dispatch( struct shader_translator *translator,
                               struct ureg_program *ureg,
                               const DWORD *tokens, )
{
    struct d3d_inst *inst = NULL;
    unsigned opcode, minver, maxver;

    opcode = INSTRUCTION_OPCODE(*tokens);

    /* find instruction handler */
    if (opcode < sizeof(inst_table)/sizeof(*inst_table)) {
        if (IS_VALID_INSTRUCTION(inst_table[opcode])) {
            inst = &inst_table[opcode];
        }
    } else {
        switch (opcode) {
            case D3DSIO_PHASE:
                inst = &inst_phase;
                break;

            case D3DSIO_COMMENT:
                inst = &inst_comment;
                break;

            default:
                break;
        }
    }

    if (!inst) {
        DBG("Unknown shader opcode 0x%x\n", opcode);
        return D3DERR_INVALIDCALL;
    }

    /* check versions */
    switch (translator->processor) {
        case TGSI_PROCESSOR_VERTEX:
            minver = inst->vert_version.min;
            maxver = inst->vert_version.max;
            break;

        case TGSI_PROCESSOR_FRAGMENT:
            minver = inst->frag_version.min;
            maxver = inst->frag_version.max;
            break;

        default:
            assert(!"Implementation error!");
    }

    if (TRANS_VER < minver || TRANS_VER > maxver) {
        DBG("Shader instruction %s found, but it does not fall between the "
            "supported versions: %u.%u (expected: %u.%u <= ver <= %u.%u)",
            d3dsio_to_string(opcode), translator->major, translator->minor,
            VMAJ(minver), VMIN(minver), VMAJ(maxver), VMIN(maxver));

        if (!QUIRK(LENIENT_SHADER)) {
            /* XXX is this quirk necessary? */
            return D3DERR_INVALIDCALL;
        }
    }

    /* call instruction handler */
    if (inst->handler) {
        return inst->handler(translator, ureg, tokens);
    } else {
        return NineTranslateInstructon_Generic(translator, ureg, tokens, inst);
    }
}

HRESULT
nine_translate_shader( const DWORD *tokens,
                       unsigned processor,
                       struct ureg_program **program )
{
    struct shader_translator translator;
    unsigned token_processor;

    translator.major = VERSION_MAJOR(*tokens);
    translator.minor = VERSION_MINOR(*tokens);
    token_processor = VERSION_TYPE(*tokens);
    ++tokens;

    switch (token_processor) {
        case D3DSHADER_VERTEX:
            user_assert(processor == TGSI_PROCESSOR_VERTEX, NULL);
            break;

        case D3DSHADER_FRAGMENT:
            user_assert(processor == TGSI_PROCESSOR_FRAGMENT, NULL);
            break;

        default:
            user_assert(!"Unknown shader version token.", NULL);
    }

    translator.processor = processor;

    *program = ureg_create(processor);
    if (!*program) { return E_OUTOFMEMORY; }

    while (INSTRUCTION_OPCODE(*tokens) != D3DSIO_END) {
        HRESULT r = NineTranslateShader_Dispatch(&translator, *program, tokens);
        assert(r != 0);

        if (r < 0) {
            ureg_destroy(*program);
            *program = NULL;
            return r;
        }

        tokens += r;
    }

    return D3D_OK;
}
