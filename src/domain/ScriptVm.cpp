#include "domain/ScriptVm.h"

namespace AquaLook { namespace Domain {

const char* scriptAbortName(ScriptAbort reason) {
    switch (reason) {
        case ScriptAbort::NONE:            return "aucune";
        case ScriptAbort::STEP_LIMIT:      return "trop d'instructions";
        case ScriptAbort::LOOP_LIMIT:      return "boucle sans fin";
        case ScriptAbort::TIME_LIMIT:      return "duree depassee";
        case ScriptAbort::STACK_OVERFLOW:  return "pile pleine";
        case ScriptAbort::STACK_UNDERFLOW: return "pile vide";
        case ScriptAbort::BAD_OPCODE:      return "instruction inconnue";
        case ScriptAbort::BAD_JUMP:        return "saut hors programme";
        case ScriptAbort::BAD_VARIABLE:    return "variable inconnue";
        case ScriptAbort::DIVIDE_BY_ZERO:  return "division par zero";
        case ScriptAbort::HOST_REFUSED:    return "action refusee";
    }
    return "inconnue";
}

// Longueur en octets des operandes de chaque instruction. -1 = inconnue.
static int8_t operandBytes(ScriptOp op) {
    switch (op) {
        case ScriptOp::HALT: case ScriptOp::DROP:
        case ScriptOp::ADD: case ScriptOp::SUB: case ScriptOp::MUL:
        case ScriptOp::DIV: case ScriptOp::MOD: case ScriptOp::NEG:
        case ScriptOp::EQ:  case ScriptOp::NE:  case ScriptOp::LT:
        case ScriptOp::LE:  case ScriptOp::GT:  case ScriptOp::GE:
        case ScriptOp::AND: case ScriptOp::OR:  case ScriptOp::NOT:
        case ScriptOp::WAIT:
            return 0;
        case ScriptOp::LOAD: case ScriptOp::STORE:
            return 1;
        case ScriptOp::JMP: case ScriptOp::JZ: case ScriptOp::JNZ:
        case ScriptOp::READ_INPUT: case ScriptOp::ZONE_ACTIVE:
        case ScriptOp::ZONE_REMAIN: case ScriptOp::NOTIFY:
        case ScriptOp::ALERT:
            return 2;
        case ScriptOp::PUSH:
            return 4;
        case ScriptOp::ACTION:
            return 3;   // action (1) + cible (2)
        default:
            return -1;
    }
}

ScriptAbort validateScriptProgram(const ScriptProgram& program) {
    if (!program.code || program.size == 0U) return ScriptAbort::BAD_OPCODE;

    // Premiere passe : parcourir les instructions et relever leurs debuts.
    // Un saut ne doit pas seulement tomber DANS le programme, il doit tomber
    // sur une frontiere d'instruction -- sinon il atterrit au milieu d'une
    // constante, et la suite est interpretee n'importe comment.
    bool boundary[512];
    const uint16_t limit = program.size < 512U ? program.size : 512U;
    for (uint16_t i = 0U; i < limit; ++i) boundary[i] = false;
    if (program.size > 512U) return ScriptAbort::BAD_JUMP;   // au-dela, on refuse

    uint16_t pc = 0U;
    while (pc < program.size) {
        boundary[pc] = true;
        const ScriptOp op = static_cast<ScriptOp>(program.code[pc]);
        const int8_t operands = operandBytes(op);
        if (operands < 0) return ScriptAbort::BAD_OPCODE;
        if (static_cast<uint32_t>(pc) + 1U + operands > program.size) {
            return ScriptAbort::BAD_JUMP;   // operande tronquee
        }
        if (op == ScriptOp::LOAD || op == ScriptOp::STORE) {
            if (program.code[pc + 1U] >= ScriptVm::VAR_COUNT) {
                return ScriptAbort::BAD_VARIABLE;
            }
        }
        pc = static_cast<uint16_t>(pc + 1U + operands);
    }
    if (pc != program.size) return ScriptAbort::BAD_JUMP;   // deborde exactement

    // Seconde passe : les cibles de saut.
    pc = 0U;
    while (pc < program.size) {
        const ScriptOp op = static_cast<ScriptOp>(program.code[pc]);
        const int8_t operands = operandBytes(op);
        if (op == ScriptOp::JMP || op == ScriptOp::JZ || op == ScriptOp::JNZ) {
            const uint16_t target = static_cast<uint16_t>(
                program.code[pc + 1U] |
                (static_cast<uint16_t>(program.code[pc + 2U]) << 8));
            if (target >= program.size || !boundary[target]) {
                return ScriptAbort::BAD_JUMP;
            }
        }
        pc = static_cast<uint16_t>(pc + 1U + operands);
    }

    return ScriptAbort::NONE;
}

void ScriptVm::load(const ScriptProgram& program,
                    const ScriptHostOps* host,
                    void* hostContext,
                    const ScriptLimits& limits) {
    _program = program;
    _host = host;
    _hostCtx = hostContext;
    _limits = limits;

    _sp = 0U;
    _pc = 0U;
    for (uint8_t i = 0U; i < VAR_COUNT; ++i) _vars[i] = 0;
    _steps = 0U;
    _loops = 0U;
    _wakeAtMs = 0U;
    _startedMs = (host && host->nowMs) ? host->nowMs(hostContext) : 0U;
    _abort = ScriptAbort::NONE;
    _status = (program.code && program.size > 0U)
        ? ScriptStatus::READY : ScriptStatus::FINISHED;
}

bool ScriptVm::push(int32_t v) {
    if (_sp >= STACK_CAPACITY) { fail(ScriptAbort::STACK_OVERFLOW); return false; }
    _stack[_sp++] = v;
    return true;
}

bool ScriptVm::pop(int32_t& v) {
    if (_sp == 0U) { fail(ScriptAbort::STACK_UNDERFLOW); return false; }
    v = _stack[--_sp];
    return true;
}

bool ScriptVm::fetch8(uint8_t& v) {
    if (_pc >= _program.size) { fail(ScriptAbort::BAD_JUMP); return false; }
    v = _program.code[_pc++];
    return true;
}

bool ScriptVm::fetch16(uint16_t& v) {
    if (static_cast<uint32_t>(_pc) + 2U > _program.size) {
        fail(ScriptAbort::BAD_JUMP);
        return false;
    }
    v = static_cast<uint16_t>(_program.code[_pc] |
        (static_cast<uint16_t>(_program.code[_pc + 1U]) << 8));
    _pc = static_cast<uint16_t>(_pc + 2U);
    return true;
}

bool ScriptVm::fetch32(int32_t& v) {
    if (static_cast<uint32_t>(_pc) + 4U > _program.size) {
        fail(ScriptAbort::BAD_JUMP);
        return false;
    }
    uint32_t raw = 0U;
    for (uint8_t i = 0U; i < 4U; ++i) {
        raw |= static_cast<uint32_t>(_program.code[_pc + i]) << (8U * i);
    }
    _pc = static_cast<uint16_t>(_pc + 4U);
    v = static_cast<int32_t>(raw);
    return true;
}

void ScriptVm::fail(ScriptAbort reason) {
    _abort = reason;
    _status = ScriptStatus::ABORTED;
}

ScriptStatus ScriptVm::tick() {
    if (_status == ScriptStatus::FINISHED || _status == ScriptStatus::ABORTED) {
        return _status;
    }
    if (!_program.code || !_host) {
        fail(ScriptAbort::BAD_OPCODE);
        return _status;
    }

    const uint32_t now = _host->nowMs ? _host->nowMs(_hostCtx) : 0U;

    // Une attente ne consomme aucune instruction : c'est ce qui rend
    // "tant que la cuve est vide { attendre 1s }" praticable sans que la
    // boucle coute quoi que ce soit au module.
    if (_status == ScriptStatus::WAITING) {
        if (static_cast<int32_t>(now - _wakeAtMs) < 0) return _status;
        _status = ScriptStatus::RUNNING;
    }

    // Le plafond de duree couvre l'execution ENTIERE, attentes comprises :
    // un script en pause devant une cuve qui ne se remplit jamais doit finir
    // par renoncer, sinon il tient une zone indefiniment.
    if (_limits.maxRunMs != 0U &&
        static_cast<uint32_t>(now - _startedMs) > _limits.maxRunMs) {
        fail(ScriptAbort::TIME_LIMIT);
        return _status;
    }

    _status = ScriptStatus::RUNNING;

    for (uint16_t budget = 0U; budget < _limits.stepsPerTick; ++budget) {
        if (++_steps > _limits.maxTotalSteps) { fail(ScriptAbort::STEP_LIMIT); return _status; }
        if (_pc >= _program.size) { fail(ScriptAbort::BAD_JUMP); return _status; }

        const uint16_t here = _pc;
        uint8_t raw = 0U;
        if (!fetch8(raw)) return _status;
        const ScriptOp op = static_cast<ScriptOp>(raw);

        int32_t a = 0, b = 0;
        uint8_t u8 = 0U;
        uint16_t u16 = 0U;

        switch (op) {
            case ScriptOp::HALT:
                _status = ScriptStatus::FINISHED;
                return _status;

            case ScriptOp::PUSH:
                if (!fetch32(a) || !push(a)) return _status;
                break;

            case ScriptOp::LOAD:
                if (!fetch8(u8)) return _status;
                if (u8 >= VAR_COUNT) { fail(ScriptAbort::BAD_VARIABLE); return _status; }
                if (!push(_vars[u8])) return _status;
                break;

            case ScriptOp::STORE:
                if (!fetch8(u8)) return _status;
                if (u8 >= VAR_COUNT) { fail(ScriptAbort::BAD_VARIABLE); return _status; }
                if (!pop(a)) return _status;
                _vars[u8] = a;
                break;

            case ScriptOp::DROP:
                if (!pop(a)) return _status;
                break;

            case ScriptOp::ADD: case ScriptOp::SUB: case ScriptOp::MUL:
            case ScriptOp::DIV: case ScriptOp::MOD:
            case ScriptOp::EQ:  case ScriptOp::NE:  case ScriptOp::LT:
            case ScriptOp::LE:  case ScriptOp::GT:  case ScriptOp::GE:
            case ScriptOp::AND: case ScriptOp::OR: {
                if (!pop(b) || !pop(a)) return _status;
                int32_t r = 0;
                switch (op) {
                    case ScriptOp::ADD: r = a + b; break;
                    case ScriptOp::SUB: r = a - b; break;
                    case ScriptOp::MUL: r = a * b; break;
                    case ScriptOp::DIV:
                        if (b == 0) { fail(ScriptAbort::DIVIDE_BY_ZERO); return _status; }
                        r = a / b; break;
                    case ScriptOp::MOD:
                        if (b == 0) { fail(ScriptAbort::DIVIDE_BY_ZERO); return _status; }
                        r = a % b; break;
                    case ScriptOp::EQ: r = (a == b); break;
                    case ScriptOp::NE: r = (a != b); break;
                    case ScriptOp::LT: r = (a <  b); break;
                    case ScriptOp::LE: r = (a <= b); break;
                    case ScriptOp::GT: r = (a >  b); break;
                    case ScriptOp::GE: r = (a >= b); break;
                    case ScriptOp::AND: r = (a != 0 && b != 0); break;
                    default:            r = (a != 0 || b != 0); break;
                }
                if (!push(r)) return _status;
                break;
            }

            case ScriptOp::NEG:
                if (!pop(a) || !push(-a)) return _status;
                break;

            case ScriptOp::NOT:
                if (!pop(a) || !push(a == 0 ? 1 : 0)) return _status;
                break;

            case ScriptOp::JMP:
            case ScriptOp::JZ:
            case ScriptOp::JNZ: {
                if (!fetch16(u16)) return _status;
                bool take = true;
                if (op != ScriptOp::JMP) {
                    if (!pop(a)) return _status;
                    take = (op == ScriptOp::JZ) ? (a == 0) : (a != 0);
                }
                if (!take) break;
                if (u16 >= _program.size) { fail(ScriptAbort::BAD_JUMP); return _status; }
                // Un saut ARRIERE est une iteration de boucle. Les compter est
                // ce qui permet de reconnaitre une boucle sans fin sans avoir
                // a comprendre le programme.
                if (u16 <= here && ++_loops > _limits.maxLoopIterations) {
                    fail(ScriptAbort::LOOP_LIMIT);
                    return _status;
                }
                _pc = u16;
                break;
            }

            case ScriptOp::READ_INPUT:
            case ScriptOp::ZONE_ACTIVE:
            case ScriptOp::ZONE_REMAIN: {
                if (!fetch16(u16)) return _status;
                int32_t value = 0;
                bool ok = false;
                if (op == ScriptOp::READ_INPUT) {
                    ok = _host->readInput && _host->readInput(_hostCtx, u16, value);
                } else if (op == ScriptOp::ZONE_ACTIVE) {
                    ok = _host->zoneActive && _host->zoneActive(_hostCtx, u16, value);
                } else {
                    ok = _host->zoneRemainingSec &&
                         _host->zoneRemainingSec(_hostCtx, u16, value);
                }
                if (!ok) { fail(ScriptAbort::HOST_REFUSED); return _status; }
                if (!push(value)) return _status;
                break;
            }

            case ScriptOp::ACTION: {
                uint8_t actionRaw = 0U;
                if (!fetch8(actionRaw) || !fetch16(u16)) return _status;
                if (!pop(a)) return _status;
                const ScriptAction act = static_cast<ScriptAction>(actionRaw);
                if (!_host->action || !_host->action(_hostCtx, act, u16, a)) {
                    fail(ScriptAbort::HOST_REFUSED);
                    return _status;
                }
                break;
            }

            case ScriptOp::NOTIFY:
                if (!fetch16(u16)) return _status;
                if (!_host->notify || !_host->notify(_hostCtx, u16)) {
                    fail(ScriptAbort::HOST_REFUSED);
                    return _status;
                }
                break;

            case ScriptOp::ALERT:
                if (!fetch16(u16)) return _status;
                if (!_host->alert || !_host->alert(_hostCtx, u16)) {
                    fail(ScriptAbort::HOST_REFUSED);
                    return _status;
                }
                break;

            case ScriptOp::WAIT: {
                if (!pop(a)) return _status;
                if (a < 0) a = 0;
                _wakeAtMs = now + static_cast<uint32_t>(a) * 1000UL;
                _status = ScriptStatus::WAITING;
                return _status;
            }

            default:
                fail(ScriptAbort::BAD_OPCODE);
                return _status;
        }
    }

    return _status;   // budget du tour epuise, on rend la main
}

}} // namespace AquaLook::Domain
