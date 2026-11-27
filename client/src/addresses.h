// Game addresses the client reads and writes. Sources (RogueSquadron64Recomp @ 222947ee):
//   docs/reference/data-structures.md, mods/multiplayer-native/src/session/ghost_lobby.cpp and world_sync.cpp,
//   tools/web/save-editor/save_editor.js, syms/rogue_squadron.syms.toml.
// Anything marked SPIKE is used on an unverified assumption; see docs/BIBLE.md section 11.
#pragma once
#include <cstdint>

namespace rs64ap::addr {

// gGameSettings 0x80130B40
constexpr uint32_t kCurrentLevel = 0x80130B40u;   // u8, enum Level
constexpr uint32_t kSettingsWord = 0x80130B4Cu;   // u32 unlock/config word: upgrades 0x1FE00, level/craft unlocks up to 0x400000
constexpr uint32_t kCheatFlags = 0x80130B58u;     // u32 gActiveCheatFlags[0]
constexpr uint32_t kNabooKey = 0x80130B68u;       // u32 gNabooStarfighterCheatHash

// gMissionState 0x80130B10
constexpr uint32_t kLives = 0x80130B10u;          // u8
constexpr uint32_t kSecondary = 0x80130B12u;      // u8 secondary weapon count
constexpr uint32_t kSecondaryMax = 0x80130B13u;   // u8
constexpr uint32_t kObjectivesDone = 0x80130B15u; // u8 completedObjectiveFlags
constexpr uint32_t kObjectivesHidden = 0x80130B16u;
constexpr uint32_t kObjectiveCount = 0x80130B17u; // u8 numMissionObjectives
constexpr uint32_t kActiveUnlocks = 0x80130B18u;  // u32, mirrors kSettingsWord bits 9..22

// Save body 0x8013A5C0 / pilots
constexpr uint32_t kSaveNabooKey = 0x8013A5C0u + 0xACu;
constexpr uint32_t kAccounts = 0x8013A5D4u;       // account_data[3], stride 0x14
constexpr uint32_t kAccountStride = 0x14u;
constexpr uint32_t kAcctCurrentLevel = 0x05u;     // u8
constexpr uint32_t kAcctMedals = 0x06u;           // 2 bits per level: byte 0x06 + (level >> 2), shift (level & 3) * 2
constexpr uint32_t kAcctUnlockWord = 0x0Cu;       // u32, upgrades + level/craft unlocks

// Masks
constexpr uint32_t kUpgradeMask = 0x0001FE00u;
constexpr uint32_t kUnlockMask = 0x007FFE00u;     // every unlock bit the game keeps per pilot (upgrades + level/craft)
constexpr uint32_t kNabooKeyValue = 0x050FE09Cu;  // CRC of !YNGWIE!

// Cheat bits (gActiveCheatFlags[0])
constexpr uint32_t kCheatGameflo = 0x1u;          // all missions
constexpr uint32_t kCheatFarmboy = 0x4u;          // Millennium Falcon
constexpr uint32_t kCheatTiedup = 0x8u;           // TIE Interceptor
constexpr uint32_t kCheatHalifax = 0x10u;         // Naboo step 1
constexpr uint32_t kCheatChicken = 0x2000000u;    // AT-ST level
constexpr uint32_t kCheatWomprat = 0x4000000u;    // Beggar's Canyon (Tatooine race)  SPIKE S2
constexpr uint32_t kCheatWoisthan = 0x8000000u;   // Death Star Trench Run           SPIKE S2
constexpr uint32_t kCheatDeaddack = 0x10000000u;  // Battle of Hoth                   SPIKE S2
constexpr uint32_t kLogicCheats = kCheatGameflo | kCheatFarmboy | kCheatTiedup | kCheatHalifax | kCheatChicken |
                                  kCheatWomprat | kCheatWoisthan | kCheatDeaddack;

// Player 1
constexpr uint32_t kPlayerCraft = 0x80137DBCu;
constexpr uint32_t kPlayerHealth = 0x80137E7Cu;   // f32, record +0xC0
constexpr uint32_t kPlayerHealthMax = 0x80137E80u;
constexpr uint32_t kPlayerSpawnShield = 0x80137DBCu + 0xC8u;

// In-mission UI state
constexpr uint32_t kPauseState = 0x8010CA20u;     // u32, 5 = paused

// Menu overlay: mission select
constexpr uint32_t kMsCursor = 0x800CDAC1u;       // u8, stock level id under the cursor
constexpr uint32_t kMsPort = 0x800CDAC8u;         // u8, controller port driving the screen
constexpr uint32_t kMsUnlocked = 0x800CDAD0u;     // u32, selectable levels  SPIKE S2

// Menu overlay: hangar (craft select)
constexpr uint32_t kHangarPort = 0x800CD6D0u;     // u8
constexpr uint32_t kHangarAllowed = 0x800CD6D8u;  // u32, bit n = craft n may launch  SPIKE S3

// Pads
constexpr uint32_t kPressedWords = 0x8013A960u;   // u32 per port: buttons pressed this frame
constexpr uint32_t kButtonsConfirm = 0x9000u;     // A | START

// Functions (vram)
constexpr uint32_t kFnApplyPlayerCraftDamage = 0x800B13E0u;
constexpr uint32_t kFnCraftMaskConsiderUnlocks = 0x800C6234u;  // menu overlay
constexpr uint32_t kFnCraftMaskIgnoreUnlocks = 0x800C63C0u;    // menu overlay
constexpr uint32_t kFnSetFontTextSlot = 0x80063CFCu;           // (font, str, slot, x) + stack y
constexpr uint32_t kFnSetFontSlotAlpha = 0x80061C74u;          // (font, slot, alpha)

inline uint32_t account(int slot) { return kAccounts + kAccountStride * (uint32_t)slot; }

}  // namespace rs64ap::addr
