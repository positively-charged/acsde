#include <stdio.h>
#include <stdint.h>

#include "task.h"
#include "pcode.h"

#define F_LATENT 0x8000u
#define F_ALL F_LATENT
#define F_ISSET( info, flag ) ( !! ( info & flag ) )
#define F_GETOPCODE( info ) ( info & ~F_ALL )

struct setup {
   struct task* task;
   struct func* func;
   struct expr* empty_string_expr;
   const char* format;
};

static void init_setup( struct setup* setup, struct task* task );
static void setup_deds( struct setup* setup );
static void setup_func( struct setup* setup, size_t entry );
static void setup_formats( struct setup* setup );
static void setup_exts( struct setup* setup );
static void setup_interns( struct setup* setup );
static void setup_return_type( struct setup* setup );
static void setup_param_list( struct setup* setup );
//static void setup_param_list_acs( struct setup* setup );
static void setup_param_list_bcs( struct setup* setup );
static void setup_default_value( struct setup* setup, struct param* param,
   i32 param_number );
static void setup_empty_string_default_value( struct setup* setup,
   struct param* param );
static void append_param( struct setup* setup, struct param* param );

struct {
   const char* name;
   const char* format;
} g_funcs[] = {
   // Format:
   // [<return-type>] [; <required-parameters> [; <optional-parameters> ]]
   { "Delay", ";i" },
   { "Random", "i;ii" },
   { "ThingCount", "i;i;i" },
   { "TagWait", ";i" },
   { "PolyWait", ";i" },
   { "ChangeFloor", ";is" },
   { "ChangeCeiling", ";is" },
   { "LineSide", "i" },
   { "ScriptWait", ";i" },
   { "ClearLineSpecial", "" },
   { "PlayerCount", "i" },
   { "GameType", "i" },
   { "GameSkill", "i" },
   { "Timer", "i" },
   { "SectorSound", ";si" },
   { "AmbientSound", ";si" },
   { "SoundSequence", ";s" },
   { "SetLineTexture", ";iiis" },
   { "SetLineBlocking", ";ii" },
   { "SetLineSpecial", ";ii;rrrrr" },
   { "ThingSound", ";isi" },
   { "ActivatorSound", ";si" },
   { "LocalAmbientSound", ";si" },
   { "SetLineMonsterBlocking", ";ii" },
   { "IsNetworkGame", "b" },
   { "PlayerTeam", "i" },
   { "PlayerHealth", "i" },
   { "PlayerArmorPoints", "i" },
   { "PlayerFrags", "i" },
   { "BlueCount", "i" },
   { "BlueTeamCount", "i" },
   { "RedCount", "i" },
   { "RedTeamCount", "i" },
   { "BlueScore", "i" },
   { "BlueTeamScore", "i" },
   { "RedScore", "i" },
   { "RedTeamScore", "i" },
   { "IsOneFlagCtf", "b" },
   { "GetInvasionWave", "i" },
   { "GetInvasionState", "i" },
   { "Music_Change", ";si" },
   { "ConsoleCommand", ";s;ii" },
   { "SinglePlayer", "b" },
   { "FixedMul", "f;ff" },
   { "FixedDiv", "f;ff" },
   { "SetGravity", ";f" },
   { "SetAirControl", ";f" },
   { "ClearInventory", "" },
   { "GiveInventory", ";si" },
   { "TakeInventory", ";si" },
   { "CheckInventory", "i;s" },
   { "Spawn", "i;sfff;ii" },
   { "SpawnSpot", "i;si;ii" },
   { "SetMusic", ";s;ii" },
   { "LocalSetMusic", ";s;ii" },
   { "SetFont", ";s" },
   { "SetThingSpecial", ";ii;rrrrr" },
   { "FadeTo", ";iiiff" },
   { "FadeRange", ";iiifiiiff" },
   { "CancelFade", "" },
   { "PlayMovie", "i;s" },
   { "SetFloorTrigger", ";iii;rrrrr" },
   { "SetCeilingTrigger", ";iii;rrrrr" },
   { "GetActorX", "f;i" },
   { "GetActorY", "f;i" },
   { "GetActorZ", "f;i" },
   { "Sin", "f;f" },
   { "Cos", "f;f" },
   { "VectorAngle", "f;ff" },
   { "CheckWeapon", "b;s" },
   { "SetWeapon", "b;s" },
   { "SetMarineWeapon", ";ii" },
   { "SetActorProperty", ";iir" },
   { "GetActorProperty", "r;ii" },
   { "PlayerNumber", "i" },
   { "ActivatorTid", "i" },
   { "SetMarineSprite", ";is" },
   { "GetScreenWidth", "i" },
   { "GetScreenHeight", "i" },
   { "Thing_Projectile2", ";iiiiiii" },
   { "StrLen", "i;s" },
   { "SetHudSize", ";iib" },
   { "GetCvar", "i;s" },
   { "SetResultValue", ";i" },
   { "GetLinerowOffset", "i" },
   { "GetActorFloorZ", "f;i" },
   { "GetActorAngle", "f;i" },
   { "GetSectorFloorZ", "f;iii" },
   { "GetSectorCeilingZ", "f;iii" },
   { "GetSigilPieces", "i" },
   { "GetLevelInfo", "i;i" },
   { "ChangeSky", ";ss" },
   { "PlayerInGame", "b;i" },
   { "PlayerIsBot", "b;i" },
   { "setcameratotexture", ";isi" },
   { "GetAmmoCapacity", "i;s" },
   { "SetAmmoCapacity", ";si" },
   { "SetActorAngle", ";if" },
   { "SpawnProjectile", ";isiiiii" },
   { "GetSectorLightLevel", "i;i" },
   { "GetActorCeilingZ", "f;i" },
   { "SetActorPosition", "b;ifffb" },
   { "ClearActorInventory", ";i" },
   { "GiveActorInventory", ";isi" },
   { "TakeActorInventory", ";isi" },
   { "CheckActorInventory", "i;is" },
   { "ThingCountName", "i;si" },
   { "SpawnSpotFacing", "i;si;i" },
   { "PlayerClass", "i;i" },
   { "GetPlayerInfo", "i;ii" },
   { "ChangeLevel", ";sii;i" },
   { "SectorDamage", ";iissi" },
   { "ReplaceTextures", ";ss;i" },
   { "GetActorPitch", "f;i" },
   { "SetActorPitch", ";if" },
   { "SetActorState", "i;is;b" },
   { "Thing_Damage2", "i;iis" },
   { "UseInventory", "i;s" },
   { "UseActorInventory", "i;is" },
   { "CheckActorCeilingTexture", "b;is" },
   { "CheckActorFloorTexture", "b;is" },
   { "GetActorLightLevel", "i;i" },
   { "SetMugShotState", ";s" },
   { "ThingCountSector", "i;iii" },
   { "ThingCountNameSector", "i;sii" },
   { "CheckPlayerCamera", "i;i" },
   { "MorphActor", "i;i;ssiiss" },
   { "UnmorphActor", "i;i;i" },
   { "GetPlayerInput", "i;ii" },
   { "ClassifyActor", "i;i" },
   { "NamedScriptWait", ";s" },
/*
   // Internal functions (Must be last.)
   // -----------------------------------------------------------------------

*/
};

struct {
   u16 opcode;
} g_deds[] = {
   { PCD_DELAY },
   { PCD_RANDOM },
   { PCD_THINGCOUNT },
   { PCD_TAGWAIT },
   { PCD_POLYWAIT },
   { PCD_CHANGEFLOOR },
   { PCD_CHANGECEILING },
   { PCD_LINESIDE },
   { PCD_SCRIPTWAIT },
   { PCD_CLEARLINESPECIAL },
   { PCD_PLAYERCOUNT },
   { PCD_GAMETYPE },
   { PCD_GAMESKILL },
   { PCD_TIMER },
   { PCD_SECTORSOUND },
   { PCD_AMBIENTSOUND },
   { PCD_SOUNDSEQUENCE },
   { PCD_SETLINETEXTURE },
   { PCD_SETLINEBLOCKING },
   { PCD_SETLINESPECIAL },
   { PCD_THINGSOUND },
   { PCD_ACTIVATORSOUND },
   { PCD_LOCALAMBIENTSOUND },
   { PCD_SETLINEMONSTERBLOCKING },
   { PCD_ISNETWORKGAME },
   { PCD_PLAYERTEAM },
   { PCD_PLAYERHEALTH },
   { PCD_PLAYERARMORPOINTS },
   { PCD_PLAYERFRAGS },
   { PCD_BLUETEAMCOUNT },
   { PCD_BLUETEAMCOUNT },
   { PCD_REDTEAMCOUNT },
   { PCD_REDTEAMCOUNT },
   { PCD_BLUETEAMSCORE },
   { PCD_BLUETEAMSCORE },
   { PCD_REDTEAMSCORE },
   { PCD_REDTEAMSCORE },
   { PCD_ISONEFLAGCTF },
   { PCD_GETINVASIONWAVE },
   { PCD_GETINVASIONSTATE },
   { PCD_MUSICCHANGE },
   { PCD_CONSOLECOMMAND },
   { PCD_SINGLEPLAYER },
   { PCD_FIXEDMUL },
   { PCD_FIXEDDIV },
   { PCD_SETGRAVITY },
   { PCD_SETAIRCONTROL },
   { PCD_CLEARINVENTORY },
   { PCD_GIVEINVENTORY },
   { PCD_TAKEINVENTORY },
   { PCD_CHECKINVENTORY },
   { PCD_SPAWN },
   { PCD_SPAWNSPOT },
   { PCD_SETMUSIC },
   { PCD_LOCALSETMUSIC },
   { PCD_SETFONT },
   { PCD_SETTHINGSPECIAL },
   { PCD_FADETO },
   { PCD_FADERANGE },
   { PCD_CANCELFADE },
   { PCD_PLAYMOVIE },
   { PCD_SETFLOORTRIGGER },
   { PCD_SETCEILINGTRIGGER },
   { PCD_GETACTORX },
   { PCD_GETACTORY },
   { PCD_GETACTORZ },
   { PCD_SIN },
   { PCD_COS },
   { PCD_VECTORANGLE },
   { PCD_CHECKWEAPON },
   { PCD_SETWEAPON },
   { PCD_SETMARINEWEAPON },
   { PCD_SETACTORPROPERTY },
   { PCD_GETACTORPROPERTY },
   { PCD_PLAYERNUMBER },
   { PCD_ACTIVATORTID },
   { PCD_SETMARINESPRITE },
   { PCD_GETSCREENWIDTH },
   { PCD_GETSCREENHEIGHT },
   { PCD_THINGPROJECTILE2 },
   { PCD_STRLEN },
   { PCD_SETHUDSIZE },
   { PCD_GETCVAR },
   { PCD_SETRESULTVALUE },
   { PCD_GETLINEROWOFFSET },
   { PCD_GETACTORFLOORZ },
   { PCD_GETACTORANGLE },
   { PCD_GETSECTORFLOORZ },
   { PCD_GETSECTORCEILINGZ },
   { PCD_GETSIGILPIECES },
   { PCD_GETLEVELINFO },
   { PCD_CHANGESKY },
   { PCD_PLAYERINGAME },
   { PCD_PLAYERISBOT },
   { PCD_SETCAMERATOTEXTURE },
   { PCD_GETAMMOCAPACITY },
   { PCD_SETAMMOCAPACITY },
   { PCD_SETACTORANGLE },
   { PCD_SPAWNPROJECTILE },
   { PCD_GETSECTORLIGHTLEVEL },
   { PCD_GETACTORCEILINGZ },
   { PCD_SETACTORPOSITION },
   { PCD_CLEARACTORINVENTORY },
   { PCD_GIVEACTORINVENTORY },
   { PCD_TAKEACTORINVENTORY },
   { PCD_CHECKACTORINVENTORY },
   { PCD_THINGCOUNTNAME },
   { PCD_SPAWNSPOTFACING },
   { PCD_PLAYERCLASS },
   { PCD_GETPLAYERINFO },
   { PCD_CHANGELEVEL },
   { PCD_SECTORDAMAGE },
   { PCD_REPLACETEXTURES },
   { PCD_GETACTORPITCH },
   { PCD_SETACTORPITCH },
   { PCD_SETACTORSTATE },
   { PCD_THINGDAMAGE2 },
   { PCD_USEINVENTORY },
   { PCD_USEACTORINVENTORY },
   { PCD_CHECKACTORCEILINGTEXTURE },
   { PCD_CHECKACTORFLOORTEXTURE },
   { PCD_GETACTORLIGHTLEVEL },
   { PCD_SETMUGSHOTSTATE },
   { PCD_THINGCOUNTSECTOR },
   { PCD_THINGCOUNTNAMESECTOR },
   { PCD_CHECKPLAYERCAMERA },
   { PCD_MORPHACTOR },
   { PCD_UNMORPHACTOR },
   { PCD_GETPLAYERINPUT },
   { PCD_CLASSIFYACTOR },
   { PCD_SCRIPTWAITNAMED },
};

struct {
   const char* name;
   const char* format;
   u16 id;
} g_exts[] = {
   { "GetLineUDMFInt", "i;is", EXTFUNC_GETLINEUDMFINT },
   { "GetLineUDMFFixed", "f;is", EXTFUNC_GETLINEUDMFFIXED },
   { "GetThingUDMFInt", "i;is", EXTFUNC_GETTHINGUDMFINT },
   { "GetThingUDMFFixed", "f;is", EXTFUNC_GETTHINGUDMFFIXED },
   { "GetSectorUDMFInt", "i;is", EXTFUNC_GETSECTORUDMFINT },
   { "GetSectorUDMFFixed", "f;is", EXTFUNC_GETSECTORUDMFFIXED },
   { "GetSideUDMFInt", "i;ibs", EXTFUNC_GETSIDEUDMFINT },
   { "GetSideUDMFFixed", "f;ibs", EXTFUNC_GETSIDEUDMFFIXED },
   { "GetActorVelX", "f;i", EXTFUNC_GETACTORVELX },
   { "GetActorVelY", "f;i", EXTFUNC_GETACTORVELY },
   { "GetActorVelZ", "f;i", EXTFUNC_GETACTORVELZ },
   { "SetActivator", "b;i;i", EXTFUNC_SETACTIVATOR },
   { "SetActivatorToTarget", "b;i", EXTFUNC_SETACTIVATORTOTARGET },
   { "GetActorViewHeight", "f;i", EXTFUNC_GETACTORVIEWHEIGHT },
   { "GetChar", "i;si", EXTFUNC_GETCHAR },
   { "GetAirSupply", "i;i", EXTFUNC_GETAIRSUPPLY },
   { "SetAirSupply", "b;ii", EXTFUNC_SETAIRSUPPLY },
   { "SetSkyScrollSpeed", ";if", EXTFUNC_SETSKYSCROLLSPEED },
   { "GetArmorType", "i;si", EXTFUNC_GETARMORTYPE },
   { "SpawnSpotForced", "i;si;ii", EXTFUNC_SPAWNSPOTFORCED },
   { "SpawnSpotFacingForced", "i;si;i", EXTFUNC_SPAWNSPOTFACINGFORCED },
   { "CheckActorProperty", "b;iir", EXTFUNC_CHECKACTORPROPERTY },
   { "SetActorVelocity", "b;ifffbb", EXTFUNC_SETACTORVELOCITY },
   { "SetUserVariable", ";isr", EXTFUNC_SETUSERVARIABLE },
   { "GetUserVariable", "r;is", EXTFUNC_GETUSERVARIABLE },
   { "Radius_Quake2", ";iiiiis", EXTFUNC_RADIUS_QUAKE2 },
   { "CheckActorClass", "b;is", EXTFUNC_CHECKACTORCLASS },
   { "SetUserArray", ";isir", EXTFUNC_SETUSERARRAY },
   { "GetUserArray", "r;isi", EXTFUNC_GETUSERARRAY },
   { "SoundSequenceOnActor", ";is", EXTFUNC_SOUNDSEQUENCEONACTOR },
   { "SoundSequenceOnSector", ";isi", EXTFUNC_SOUNDSEQUENCEONSECTOR },
   { "SoundSequenceOnPolyobj", ";is", EXTFUNC_SOUNDSEQUENCEONPOLYOBJ },
   { "GetPolyobjX", "f;i", EXTFUNC_GETPOLYOBJX },
   { "GetPolyobjY", "f;i", EXTFUNC_GETPOLYOBJY },
   { "CheckSight", "b;iii", EXTFUNC_CHECKSIGHT },
   { "SpawnForced", "i;sfff;ii", EXTFUNC_SPAWNFORCED },
   { "AnnouncerSound", ";si", EXTFUNC_ANNOUNCERSOUND },
   { "SetPoier", "b;ii;ii", EXTFUNC_SETPOINTER },
   { "Acs_NamedExecute", "b;si;rrr", EXTFUNC_ACSNAMEDEXECUTE },
   { "Acs_NamedSuspend", "b;si", EXTFUNC_ACSNAMEDSUSPEND },
   { "Acs_NamedTerminate", "b;si", EXTFUNC_ACSNAMEDTERMINATE },
   { "Acs_NamedLockedExecute", "b;sirrr", EXTFUNC_ACSNAMEDLOCKEDEXECUTE },
   { "Acs_NamedLockedExecuteDoor", "b;sirrr",
      EXTFUNC_ACSNAMEDLOCKEDEXECUTEDOOR },
   { "Acs_NamedExecuteWithResult", "i;s;rrrr",
      EXTFUNC_ACSNAMEDEXECUTEWITHRESULT },
   { "Acs_NamedExecuteAlways", "b;si;rrr", EXTFUNC_ACSNAMEDEXECUTEALWAYS },
   { "UniqueTid", "i;;ii", EXTFUNC_UNIQUETID },
   { "IsTidUsed", "b;i", EXTFUNC_ISTIDUSED },
   { "Sqrt", "i;i", EXTFUNC_SQRT },
   { "FixedSqrt", "f;f", EXTFUNC_FIXEDSQRT },
   { "VectorLength", "i;ii", EXTFUNC_VECTORLENGTH },
   { "SetHudClipRect", ";iiii;ib", EXTFUNC_SETHUDCLIPRECT },
   { "SetHudWrapWidth", ";i", EXTFUNC_SETHUDWRAPWIDTH },
   { "SetCVar", "b;si", EXTFUNC_SETCVAR },
   { "GetUserCVar", "i;is", EXTFUNC_GETUSERCVAR },
   { "SetUserCVar", "b;isi", EXTFUNC_SETUSERCVAR },
   { "GetCVarString", "s;s", EXTFUNC_GETCVARSTRING },
   { "SetCVarString", "b;ss", EXTFUNC_SETCVARSTRING },
   { "GetUserCVarString", "s;is", EXTFUNC_GETUSERCVARSTRING },
   { "SetUserCVarString", "b;iss", EXTFUNC_SETUSERCVARSTRING },
   { "LineAttack", ";iffi;ssfii", EXTFUNC_LINEATTACK },
   { "PlaySound", ";is;ifbfb", EXTFUNC_PLAYSOUND },
   { "StopSound", ";i;i", EXTFUNC_STOPSOUND },
   { "Strcmp", "i;ss;i", EXTFUNC_STRCMP },
   { "Stricmp", "i;ss;i", EXTFUNC_STRICMP },
   { "Strcasecmp", "i;ss;i", EXTFUNC_STRCASECMP },
   { "StrLeft", "s;si", EXTFUNC_STRLEFT },
   { "StrRight", "s;si", EXTFUNC_STRRIGHT },
   { "StrMid", "s;sii", EXTFUNC_STRMID },
   { "GetActorClass", "s;i", EXTFUNC_GETACTORCLASS },
   { "GetWeapon", "s;", EXTFUNC_GETWEAPON },
   { "SoundVolume", ";iif", EXTFUNC_SOUNDVOLUME },
   { "PlayActorSound", ";ii;ifbf", EXTFUNC_PLAYACTORSOUND },
   { "SpawnDecal", "i;is;ifff", EXTFUNC_SPAWNDECAL },
   { "CheckFont", "b;s", EXTFUNC_CHECKFONT },
   { "DropItem", "i;is;ii", EXTFUNC_DROPITEM },
   { "CheckFlag", "b;is", EXTFUNC_CHECKFLAG },
   { "SetLineActivation", ";ii", EXTFUNC_SETLINEACTIVATION },
   { "GetLineActivation", "i;i", EXTFUNC_GETLINEACTIVATION },
   { "GetActorPowerupTics", "i;is", EXTFUNC_GETACTORPOWERUPTICS },
   { "ChangeActorAngle", ";if;b", EXTFUNC_CHANGEACTORANGLE },
   { "ChangeActorPitch", ";if;b", EXTFUNC_CHANGEACTORPITCH },
   { "GetArmorInfo", "i;i", EXTFUNC_GETARMORINFO },
   { "DropInventory", ";is", EXTFUNC_DROPINVENTORY },
   { "PickActor", "b;ifffi;iib", EXTFUNC_PICKACTOR },
   { "IsPoierEqual", "b;ii;ii", EXTFUNC_ISPOINTEREQUAL },
   { "CanRaiseActor", "b;i", EXTFUNC_CANRAISEACTOR },
   { "SetActorTeleFog", ";iss", EXTFUNC_SETACTORTELEFOG },
   { "SwapActorTeleFog", "i;i", EXTFUNC_SWAPACTORTELEFOG },
   { "SetActorRoll", ";if", EXTFUNC_SETACTORROLL },
   { "ChangeActorRoll", ";if;b", EXTFUNC_CHANGEACTORROLL },
   { "GetActorRoll", "f;i", EXTFUNC_GETACTORROLL },
   { "QuakeEx", "b;iiiiiiis;ifffiiff", EXTFUNC_QUAKEEX },
   { "Warp", "b;iffffi;sbfff", EXTFUNC_WARP },
   { "GetMaxInventory", "i;is", EXTFUNC_GETMAXINVENTORY },
   { "SetSectorDamage", ";ii;sii", EXTFUNC_SETSECTORDAMAGE },
   { "SetSectorTerrain", ";iis", EXTFUNC_SETSECTORTERRAIN },
   { "SpawnParticle", ";i;biifffffffffiii", EXTFUNC_SPAWNPARTICLE },
   { "SetMusicVolume", ";f", EXTFUNC_SETMUSICVOLUME },
   { "CheckProximity", "b;ssf;iii", EXTFUNC_CHECKPROXIMITY },
   { "CheckActorState", "b;is;b", EXTFUNC_CHECKACTORSTATE },
   { "ResetMap", "b;", EXTFUNC_RESETMAP },
   { "PlayerIsSpectator", "b;i", EXTFUNC_PLAYERISSPECTATOR },
   { "ConsolePlayerNumber", "i;", EXTFUNC_CONSOLEPLAYERNUMBER },
   { "GetTeamProperty", "i;ii", EXTFUNC_GETTEAMPROPERTY },
   { "GetPlayerLivesLeft", "i;i", EXTFUNC_GETPLAYERLIVESLEFT },
   { "SetPlayerLivesLeft", "b;ii", EXTFUNC_SETPLAYERLIVESLEFT },
   { "KickFromGame", "b;is", EXTFUNC_KICKFROMGAME },
   { "GetGamemodeState", "i;", EXTFUNC_GETGAMEMODESTATE },
   { "SetDBEntry", ";ssi", EXTFUNC_SETDBENTRY },
   { "GetDBEntry", "i;ss", EXTFUNC_GETDBENTRY },
   { "SetDBEntryString", ";sss", EXTFUNC_SETDBENTRYSTRING },
   { "GetDBEntryString", "s;ss", EXTFUNC_GETDBENTRYSTRING },
   { "IncrementDBEntry", ";ssi", EXTFUNC_INCREMENTDBENTRY },
   { "PlayerIsLoggedIn", "b;i", EXTFUNC_PLAYERISLOGGEDIN },
   { "GetPlayerAccountName", "s;i", EXTFUNC_GETPLAYERACCOUNTNAME },
   { "SortDBEntries", "i;siib", EXTFUNC_SORTDBENTRIES },
   { "CountDBResults", "i;i", EXTFUNC_COUNTDBRESULTS },
   { "FreeDBResults", ";i", EXTFUNC_FREEDBRESULTS },
   { "GetDBResultKeyString", "s;ii", EXTFUNC_GETDBRESULTKEYSTRING },
   { "GetDBResultValueString", "s;ii", EXTFUNC_GETDBRESULTVALUESTRING },
   { "GetDBResultValue", "i;ii", EXTFUNC_GETDBRESULTVALUE },
   { "GetDBEntryRank", "i;ssb", EXTFUNC_GETDBENTRYRANK },
   { "RequestScriptPuke", "i;i;iiii", EXTFUNC_REQUESTSCRIPTPUKE },
   { "BeginDBTransaction", "", EXTFUNC_BEGINDBTRANSACTION },
   { "EndDBTransaction", "", EXTFUNC_ENDDBTRANSACTION },
   { "GetDBEntries", "i;s", EXTFUNC_GETDBENTRIES },
   { "NamedRequestScriptPuke", "i;s;iiii", EXTFUNC_NAMEDREQUESTSCRIPTPUKE },
   { "SystemTime", "i;", EXTFUNC_SYSTEMTIME },
   { "GetTimeProperty", "i;ii;b", EXTFUNC_GETTIMEPROPERTY },
   { "Strftime", "s;is;b", EXTFUNC_STRFTIME },
   { "CheckClass", "b;s", EXTFUNC_CHECKCLASS },
   { "DamageActor", "i;iiiiis", EXTFUNC_DAMAGEACTOR },
   { "SetActorFlag", "i;isb", EXTFUNC_SETACTORFLAG },
   { "SetTranslation", ";is", EXTFUNC_SETTRANSLATION },
   { "GetActorFloorTexture", "s;i", EXTFUNC_GETACTORFLOORTEXTURE },
   { "GetActorFloorTerrain", "s;i", EXTFUNC_GETACTORFLOORTERRAIN },
   { "StrArg", "i;s", EXTFUNC_STRARG },
   { "Floor", "f;f", EXTFUNC_FLOOR },
   { "Round", "f;f", EXTFUNC_ROUND },
   { "Ceil", "f;f", EXTFUNC_CEIL },
   { "ScriptCall", "r;ss;"
      "rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr"
      "rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr",
      EXTFUNC_SCRIPTCALL },
   { "StartSlideShow", ";s", EXTFUNC_STARTSLIDESHOW },
   { "GetLineX", "f;iff", EXTFUNC_GETLINEX },
   { "GetLineY", "f;iff", EXTFUNC_GETLINEY },
   { "SetSectorGlow", ";iiiiii", EXTFUNC_SETSECTORGLOW },
   { "SetFogDensity", ";ii", EXTFUNC_SETFOGDENSITY },
   { "GetTeamScore", "i;i", EXTFUNC_GETTEAMSCORE },
   { "SetTeamScore", ";ii", EXTFUNC_SETTEAMSCORE },
};

struct {
   const char* name;
   const char* format;
   u16 id;
} g_interns[] = {
   { "ACS_ExecuteWait", ";i;rrrr", INTERNFUNC_ACSEXECUTEWAIT },
   { "ACS_NamedExecuteWait", ";s;rrrr", INTERNFUNC_ACSNAMEDEXECUTEWAIT },
};

enum {
   BOUND_DED = ARRAY_SIZE( g_deds ),
   BOUND_DED_ACS95 = 21,
};

void t_create_builtins( struct task* task ) {
   struct setup setup;
   init_setup( &setup, task );
   enum { TOTAL_IMPLS =
      ARRAY_SIZE( g_deds ) };
   if ( ARRAY_SIZE( g_funcs ) != TOTAL_IMPLS ) {
      t_diag( task, DIAG_INTERNAL | DIAG_ERR,
         "builtin function declarations (%zu) != implementations (%zu)",
         ARRAY_SIZE( g_funcs ), TOTAL_IMPLS );
      t_bail( task );
   }
   setup_deds( &setup );
   setup_formats( &setup );
   setup_exts( &setup );
   setup_interns( &setup );
   //if ( lang == LANG_ACS95 ) {
      // Dedicated functions.
/*
      for ( int entry = 0; entry < BOUND_DED_ACS95; ++entry ) {
         setup_func( &setup, entry );
      }
      // Format functions.
      setup_func( &setup, BOUND_DED );
      setup_func( &setup, BOUND_DED + 1 );
*/
/*
   }
   else {
      for ( int entry = 0; entry < ARRAY_SIZE( g_funcs ); ++entry ) {
         setup_func( &setup, entry );
      }
   }

*/
}


struct func* t_get_ded_func( struct task* task, i32 opcode ) {
   switch ( opcode ) {
   case PCD_DELAYDIRECT:
   case PCD_DELAYDIRECTB: opcode = PCD_DELAY; break;
   case PCD_RANDOMDIRECT:
   case PCD_RANDOMDIRECTB: opcode = PCD_RANDOM; break;
   case PCD_THINGCOUNTDIRECT: opcode = PCD_THINGCOUNT; break;
   case PCD_TAGWAITDIRECT: opcode = PCD_TAGWAIT; break;
   case PCD_POLYWAITDIRECT: opcode = PCD_POLYWAIT; break;
   case PCD_CHANGEFLOORDIRECT: opcode = PCD_CHANGEFLOOR; break;
   case PCD_CHANGECEILINGDIRECT: opcode = PCD_CHANGECEILING; break;
   case PCD_SCRIPTWAITDIRECT: opcode = PCD_SCRIPTWAIT; break;
   case PCD_CONSOLECOMMANDDIRECT: opcode = PCD_CONSOLECOMMAND; break;
   case PCD_SETGRAVITYDIRECT: opcode = PCD_SETGRAVITY; break;
   case PCD_SETAIRCONTROLDIRECT: opcode = PCD_SETAIRCONTROL; break;
   case PCD_GIVEINVENTORYDIRECT: opcode = PCD_GIVEINVENTORY; break;
   case PCD_TAKEINVENTORYDIRECT: opcode = PCD_TAKEINVENTORY; break;
   case PCD_CHECKINVENTORYDIRECT: opcode = PCD_CHECKINVENTORY; break;
   case PCD_SPAWNDIRECT: opcode = PCD_SPAWN; break;
   case PCD_SPAWNSPOTDIRECT: opcode = PCD_SPAWNSPOT; break;
   case PCD_SETMUSICDIRECT: opcode = PCD_SETMUSIC; break;
   case PCD_LOCALSETMUSICDIRECT: opcode = PCD_LOCALSETMUSIC; break;
   case PCD_SETFONTDIRECT: opcode = PCD_SETFONT; break;
   default: break;
   }
   size_t index = 0;
   while ( index < ARRAY_SIZE( g_deds ) ) {
      if ( g_deds[ index ].opcode == opcode ) {
         return task->ded_funcs[ index ];
      }
      ++index;
   }
   return NULL;
}

void init_setup( struct setup* setup, struct task* task ) {
   setup->task = task;
   setup->func = NULL;
   setup->empty_string_expr = NULL;
   setup->format = NULL;
}

static void setup_deds( struct setup* setup ) {
   setup->task->ded_funcs = mem_alloc(
      sizeof( setup->task->ded_funcs[ 0 ] ) * ARRAY_SIZE( g_deds ) );
   for ( size_t i = 0; i < ARRAY_SIZE( g_deds ); ++i ) {
      setup_func( setup, i );
      setup->task->ded_funcs[ i ] = setup->func;
   }
}

void setup_func( struct setup* setup, size_t entry ) {
   struct func* func = t_alloc_func();
   str_append( &func->name, g_funcs[ entry ].name );
   // Dedicated function.
   if ( entry < BOUND_DED ) {
      struct func_ded* more = mem_alloc( sizeof( *more ) );
      more->opcode = g_deds[ entry ].opcode;
      func->type = FUNC_DED;
      func->more.ded = more;
   }
   // Internal function.
   else {
/*
      int impl_entry = entry - BOUND_FORMAT;
      struct func_intern* impl = mem_alloc( sizeof( *impl ) );
      impl->id = g_interns[ impl_entry ].id;
      func->type = FUNC_INTERNAL;
      func->impl = impl;
*/
   }
   setup->func = func;
   setup->format = g_funcs[ entry ].format;
   setup_return_type( setup );
   if ( setup->format[ 0 ] == ';' ) {
      ++setup->format;
      setup_param_list( setup );
   }
}

static const struct {
   const char* name;
   const char* format;
   i32 opcode;
} g_formats[] = {
   { "Print", "", PCD_ENDPRINT },
   { "PrintBold", "", PCD_ENDPRINTBOLD },
   { "HudMessage", ";iiifff;fff", PCD_ENDHUDMESSAGE },
   { "HudMessageBold", ";iiifff;fff", PCD_ENDHUDMESSAGEBOLD },
   { "Log", "", PCD_ENDLOG },
   { "StrParam", "s", PCD_SAVESTRING },
};

static void setup_formats( struct setup* setup ) {
   setup->task->format_funcs = mem_alloc(
      sizeof( setup->task->format_funcs[ 0 ] ) * ARRAY_SIZE( g_formats ) );
   for ( size_t i = 0; i < ARRAY_SIZE( g_formats ); ++i ) {
      struct func* func = t_alloc_func();
      func->type = FUNC_FORMAT;
      str_append( &func->name, g_formats[ i ].name );
      struct func_format* more = mem_alloc( sizeof( *more ) );
      more->opcode = g_formats[ i ].opcode;
      func->more.format = more;
      setup->task->format_funcs[ i ] = func;
      setup->func = func;
      setup->format = g_formats[ i ].format;
      setup_return_type( setup );
      if ( setup->format[ 0 ] == ';' ) {
         ++setup->format;
         setup_param_list( setup );
      }
   }
}

struct func* t_find_format_func( struct task* task, i32 opcode ) {
   size_t index = 0;
   while ( index < ARRAY_SIZE( g_formats ) ) {
      if ( g_formats[ index ].opcode == opcode ) {
         return task->format_funcs[ index ];
      }
      ++index;
   }
   return NULL;
}

static void setup_exts( struct setup* setup ) {
   setup->task->ext_funcs = mem_alloc(
      sizeof( setup->task->ext_funcs[ 0 ] ) * ARRAY_SIZE( g_exts ) );
   for ( size_t i = 0; i < ARRAY_SIZE( g_exts ); ++i ) {
      struct func* func = t_alloc_func();
      func->type = FUNC_EXT;
      str_append( &func->name, g_exts[ i ].name );
      struct func_ext* more = mem_alloc( sizeof( *more ) );
      more->id = g_exts[ i ].id;
      func->more.ext = more;
      setup->task->ext_funcs[ i ] = func;
      setup->func = func;
      setup->format = g_exts[ i ].format;
      setup_return_type( setup );
      if ( setup->format[ 0 ] == ';' ) {
         ++setup->format;
         setup_param_list( setup );
      }
   }
}

struct func* t_find_ext_func( struct task* task, i32 id ) {
   size_t index = 0;
   while ( index < ARRAY_SIZE( g_exts ) ) {
      if ( g_exts[ index ].id == id ) {
         return task->ext_funcs[ index ];
      }
      ++index;
   }
   return NULL;
}

static void setup_interns( struct setup* setup ) {
   setup->task->intern_funcs = mem_alloc(
      sizeof( setup->task->intern_funcs[ 0 ] ) * ARRAY_SIZE( g_interns ) );
   for ( size_t i = 0; i < ARRAY_SIZE( g_interns ); ++i ) {
      struct func* func = t_alloc_func();
      func->type = FUNC_INTERN;
      str_append( &func->name, g_interns[ i ].name );
      struct func_intern* more = mem_alloc( sizeof( *more ) );
      more->id = g_interns[ i ].id;
      func->more.intern = more;
      setup->task->intern_funcs[ i ] = func;
      setup->func = func;
      setup->format = g_interns[ i ].format;
      setup_return_type( setup );
      if ( setup->format[ 0 ] == ';' ) {
         ++setup->format;
         setup_param_list( setup );
      }
   }
}

struct func* t_find_intern_func( struct task* task, i32 id ) {
   size_t index = 0;
   while ( index < ARRAY_SIZE( g_interns ) ) {
      if ( g_interns[ index ].id == id ) {
         return task->intern_funcs[ index ];
      }
      ++index;
   }
   return NULL;
}

static void setup_format_func( struct setup* setup ) {
}

void setup_return_type( struct setup* setup ) {
   i32 spec = SPEC_VOID;
   if ( ! (
      setup->format[ 0 ] == '\0' ||
      setup->format[ 0 ] == ';' ) ) { 
      switch ( setup->format[ 0 ] ) {
      case 'i': spec = SPEC_INT; break;
      case 'r': spec = SPEC_RAW; break;
      case 'f': spec = SPEC_FIXED; break;
      case 'b': spec = SPEC_BOOL; break;
      case 's': spec = SPEC_STR; break;
      default:
         t_diag( setup->task, DIAG_INTERNAL | DIAG_ERR,
            "invalid builtin function return type `%c`", setup->format[ 0 ] );
         t_bail( setup->task );
      }
      //switch ( setup->lang ) {
      //case LANG_ACS:
      //case LANG_ACS95:
         if ( spec != SPEC_VOID ) {
            spec = SPEC_RAW;
         }
        // break;
      //default:
      //   break;
      //}
      ++setup->format;
   }
   setup->func->return_spec = spec;
}

void setup_param_list( struct setup* setup ) {
  // switch ( setup->lang ) {
   //case LANG_ACS:
   //case LANG_ACS95:
      //setup_param_list_acs( setup );
     // break;
   //default:
      setup_param_list_bcs( setup );
   //}
}

/*
void setup_param_list_acs( struct setup* setup ) {
   // In ACS and ACS95, we're just interested in the parameter count.
   bool optional = false;
   while ( setup->format[ 0 ] ) {
      if ( setup->format[ 0 ] == ';' ) {
         optional = true;
      }
      else {
         if ( ! optional ) {
            ++setup->func->min_param;
         }
         ++setup->func->max_param;
      }
      ++setup->format;
   }
} */

void setup_param_list_bcs( struct setup* setup ) {
   const char* format = setup->format;
   bool optional = false;
   while ( format[ 0 ] ) {
      i32 spec = SPEC_NONE;
      switch ( format[ 0 ] ) {
      case 'i': spec = SPEC_INT; break;
      case 'r': spec = SPEC_RAW; break;
      case 'f': spec = SPEC_FIXED; break;
      case 'b': spec = SPEC_BOOL; break;
      case 's': spec = SPEC_STR; break;
      case ';':
         optional = true;
         ++format;
         continue;
      default:
         t_diag( setup->task, DIAG_INTERNAL | DIAG_ERR,
            "invalid builtin function parameter `%c`", format[ 0 ] );
         t_bail( setup->task );
      }
      struct param* param = t_alloc_param();
      param->spec = spec;
      if ( optional ) {
         //setup_default_value( setup, param, setup->func->max_param );
      }
      else {
         ++setup->func->min_param;
      }
      list_append( &setup->func->params, param );
      ++setup->func->max_param;
      ++format;
   }
}

/*
void setup_default_value( struct setup* setup, struct param* param,
   int param_number ) {
   switch ( setup->func->type ) {
      struct func_ded* ded;
   case FUNC_DED:
      ded = setup->func->more.ded;
      switch ( ded->opcode ) {
      case PCD_MORPHACTOR:
         switch ( param_number ) {
         case 1: case 2: case 5: case 6:
            setup_empty_string_default_value( setup, param );
            return;
         }
      }
   default:
      break;
   }
   param->default_value = setup->task->dummy_expr;
}

void setup_empty_string_default_value( struct setup* setup,
   struct param* param ) {
   if ( ! setup->empty_string_expr ) {
      struct indexed_string* string = t_intern_string( setup->task, "", 0 );
      struct indexed_string_usage* usage = t_alloc_indexed_string_usage();
      usage->string = string;
      struct expr* expr = t_alloc_expr();
      t_init_pos_id( &expr->pos, INTERNALFILE_COMPILER );
      expr->root = &usage->node;
      expr->spec = SPEC_STR;
      expr->value = string->index;
      expr->folded = true;
      expr->has_str = true;
      setup->empty_string_expr = expr;
   }
   param->default_value = setup->empty_string_expr;
}*/
