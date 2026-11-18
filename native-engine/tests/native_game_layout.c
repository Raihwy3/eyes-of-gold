/* Fail conversion before packaging if the compiled ARM ABI disagrees. */
// clang-format off
#include <ultra64.h>
#include "game/bg.h"
#include <bondtypes.h>
#include "objecthandler.h"
// clang-format on
#define SIZE(t, n) _Static_assert(sizeof(t) == (n), #t " native stride")
_Static_assert(__builtin_offsetof(struct ModelSlot,unk08)==__builtin_offsetof(Model,obj),"ModelSlot in-use marker");
_Static_assert(__builtin_offsetof(struct ModelSlot,unk10)==__builtin_offsetof(Model,datas),"ModelSlot RW data pointer");
_Static_assert(__builtin_offsetof(struct AnimModelSlot,unk08)==__builtin_offsetof(Model,obj),"AnimModelSlot in-use marker");
_Static_assert(__builtin_offsetof(struct AnimModelSlot,unk10)==__builtin_offsetof(Model,datas),"AnimModelSlot RW data pointer");
_Static_assert(sizeof(struct ModelSlot)>=sizeof(Model),"ModelSlot storage capacity");
_Static_assert(sizeof(struct AnimModelSlot)>=sizeof(Model),"AnimModelSlot storage capacity");
SIZE(ModelNode, 24);
SIZE(ModelRoData_HeaderRecord, 16);
SIZE(ModelRoData_GroupRecord, 28);
SIZE(ModelRoData_DisplayListRecord, 20);
SIZE(ObjectRecord, 128);
SIZE(DoorRecord, 256);
SIZE(GlobalDoorScaleRecord, 8);
_Static_assert(__builtin_offsetof(GlobalDoorScaleRecord,Scale)==4,"global door scale scalar word");
SIZE(GuardRecord, 28);
SIZE(WeaponObjRecord, 136);
SIZE(BodyArmourRecord, 136);
SIZE(LinkRecord, 16);
/* Dual-weapon links serialize only the first 12 bytes; the shared next
 * slot belongs to the 16-byte switch record, not the type-14 stream. */
_Static_assert(__builtin_offsetof(LinkRecord,Index1)==4,"dual link first relative index");
_Static_assert(__builtin_offsetof(LinkRecord,Index2)==8,"dual link second relative index");
_Static_assert(__builtin_offsetof(LinkRecord,next)==12,"dual link live prefix fits three words");
SIZE(LockDoorRecord, 16);
SIZE(MonitorObjRecord, 256);
SIZE(MultiMonitorObjRecord, 596);
SIZE(TintedGlassRecord, 148);
SIZE(VehichleRecord, 176);
SIZE(AutogunRecord, 216);
SIZE(MultiAmmoCrateRecord, 180);
SIZE(AircraftRecord, 180);
SIZE(TankRecord, 224);
SIZE(SafeObjectRecord, 20);
SIZE(struct criteria_deposit, 20);
_Static_assert(__builtin_offsetof(struct criteria_deposit,weaponnum)==4,"deposit weapon word");
_Static_assert(__builtin_offsetof(struct criteria_deposit,padid)==8,"deposit pad word");
_Static_assert(__builtin_offsetof(struct criteria_deposit,flag)==12,"deposit completion word");
_Static_assert(__builtin_offsetof(struct criteria_deposit,next)==16,"deposit next pointer");
SIZE(struct criteria_picture, 16);
_Static_assert(__builtin_offsetof(struct criteria_picture,tag_id)==4,"photo target tag word");
_Static_assert(__builtin_offsetof(struct criteria_picture,flag)==8,"photo objective flag word");
_Static_assert(__builtin_offsetof(struct criteria_picture,next)==12,"photo criterion next pointer");
/* The retail CCTV stream is 59 words; the decomp's unused tail declarations
 * extend beyond it. Setup/tick/render only access through unkE8. */
_Static_assert(__builtin_offsetof(CCTVRecord,lookpad)==128,"CCTV look-at pad word");
_Static_assert(__builtin_offsetof(CCTVRecord,unk84)==132,"CCTV live basis matrix");
_Static_assert(__builtin_offsetof(CCTVRecord,unkE8)+sizeof(float)==59*4,"CCTV live fields fit serialized record");
_Static_assert(__builtin_offsetof(SafeObjectRecord,Index1)==4,"safe item index");
_Static_assert(__builtin_offsetof(SafeObjectRecord,Index2)==8,"safe index");
_Static_assert(__builtin_offsetof(SafeObjectRecord,Index3)==12,"safe door index");
_Static_assert(__builtin_offsetof(SafeObjectRecord,next)==16,"safe-link next pointer");
_Static_assert(__builtin_offsetof(AircraftRecord,aioffset)==132,"aircraft AI cursor halfword");
_Static_assert(__builtin_offsetof(AircraftRecord,aireturnlist)==134,"aircraft return-list halfword");
_Static_assert(__builtin_offsetof(MultiAmmoCrateRecord,slots)==128,"ammo crate slot pairs");
_Static_assert(__builtin_offsetof(TankRecord,collision)==128,"tank collision pointer");
SIZE(PadRecord, 44);
SIZE(BoundPadRecord, 68);
SIZE(struct stagesetup, 40);
SIZE(bg_portal_data_entry, 8);
SIZE(Gfx, 8);
SIZE(Vtx, 16);

SIZE(waypoint,16);
SIZE(waygroup,12);
SIZE(PathRecord,8);
SIZE(AIListRecord,8);
_Static_assert(__builtin_offsetof(GuardRecord,AIListID)==10,"guard AI-list field");
_Static_assert(__builtin_offsetof(GuardRecord,PadID)==6,"guard spawn pad field");

/* Bank records are endian-converted in place before original relocation. */
_Static_assert(__builtin_offsetof(ALBankFile,bankArray)==4,"ARM bank file slots");
_Static_assert(__builtin_offsetof(ALBank,instArray)==12,"ARM bank instrument slots");
_Static_assert(__builtin_offsetof(ALInstrument,soundArray)==16,"ARM instrument sounds");
_Static_assert(__builtin_offsetof(ALInstrument,flags)==3,"ARM instrument flags");
SIZE(ALSound,16);
SIZE(ALWaveTable,20);
SIZE(ALEnvelope,16);
SIZE(ALADPCMloop,44);

#include "snd.h"
_Static_assert(__builtin_offsetof(struct ALInstrumentAlt_s,soundArray)==12,"ARM sound IDs alias table one slot early");
