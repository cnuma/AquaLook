#pragma once

#include <stdint.h>

#include "domain/BinaryActuatorDriver.h"
#include "domain/ProtocolBuildProfile.h"
#include "domain/Xl9535SharedOutputState.h"

namespace AquaLook { namespace Domain {

#if AQUALOOK_V4_ENABLE_I2C

struct Xl9535I2cOps {
    bool (*probe)(void* platformContext, uint8_t address);
    bool (*writeRegister16)(
        void* platformContext,
        uint8_t address,
        uint8_t registerAddress,
        uint16_t value
    );
    bool (*readRegister16)(
        void* platformContext,
        uint8_t address,
        uint8_t registerAddress,
        uint16_t& value
    );
};

// Plan de registres d'un expandeur I2C 16 bits.
//
// XL9535 et MCP23017 se pilotent EXACTEMENT de la meme facon : trois paires
// de registres consecutifs -- lecture des entrees, latch de sortie, sens des
// broches -- adressees en 16 bits (octet bas = port A, octet haut = port B),
// et la meme convention de direction (1 = entree, 0 = sortie). Seuls les
// NUMEROS de registre different.
//
// C'est donc une donnee, pas un pilote de plus : le meme code sert les deux,
// et servira le prochain expandeur de cette famille sans une ligne de plus.
struct I2cExpanderRegisterMap {
    uint8_t input;      // lecture de l'etat reel des broches
    uint8_t output;     // latch de sortie
    uint8_t direction;  // sens des broches (1 = entree)
};

struct Xl9535BinaryActuatorContext {
    const Xl9535I2cOps* i2c;
    void* platformContext;
    Xl9535SharedOutputState* sharedOutputState;
    // Plan de registres et type attendu. Laisses a zero, le pilote se
    // comporte en XL9535 : les appelants ecrits avant l'arrivee du MCP23017
    // n'ont rien a changer.
    const I2cExpanderRegisterMap* registers;
    ControllerTypeId expectedControllerType;
    BinaryActuatorHealth health;
    uint8_t configured;
    uint8_t address;
    uint16_t outputLatch;
    uint16_t directionMask;
    BinaryActuatorState lastObserved;
    uint8_t reserved;

    constexpr Xl9535BinaryActuatorContext()
        : i2c(nullptr), platformContext(nullptr), sharedOutputState(nullptr),
          registers(nullptr), expectedControllerType(),
          health(BinaryActuatorHealth::UNKNOWN), configured(0U), address(0U),
          outputLatch(0U), directionMask(0xFFFFU),
          lastObserved(BinaryActuatorState::UNKNOWN), reserved(0U) {}
};

namespace Xl9535Registers {
constexpr uint8_t INPUT_PORT = 0x00U;
constexpr uint8_t OUTPUT_PORT = 0x02U;
constexpr uint8_t POLARITY_INVERSION = 0x04U;
constexpr uint8_t CONFIGURATION = 0x06U;
}

// MCP23017, IOCON.BANK = 0 (valeur au reset) : les registres A et B sont
// alors ADJACENTS, ce qui autorise le meme acces 16 bits que le XL9535.
//   IODIRA 0x00 / IODIRB 0x01   sens des broches
//   GPIOA  0x12 / GPIOB  0x13   etat reel
//   OLATA  0x14 / OLATB  0x15   latch de sortie
//
// Ecrire dans OLAT plutot que dans GPIO est deliberé : GPIO relit l'etat des
// broches, OLAT retient ce qui a ete demande. Sur une sortie chargee, les
// deux peuvent differer.
namespace Mcp23017Registers {
constexpr uint8_t IODIR = 0x00U;
constexpr uint8_t GPIO = 0x12U;
constexpr uint8_t OLAT = 0x14U;
}

const BinaryActuatorDriverOps& xl9535BinaryActuatorDriverOps();

BinaryActuatorDriverBinding makeXl9535BinaryActuatorDriverBinding(
    Xl9535BinaryActuatorContext& context
);

// Meme pilote, autre plan de registres. Le contexte fourni est complete par
// la fabrique : l'appelant n'a pas a connaitre les numeros de registre.
BinaryActuatorDriverBinding makeMcp23017BinaryActuatorDriverBinding(
    Xl9535BinaryActuatorContext& context
);

bool hasCompleteXl9535I2cOps(const Xl9535I2cOps& operations);

#endif

}} // namespace AquaLook::Domain
