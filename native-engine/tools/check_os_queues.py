#!/usr/bin/env python3
"""Stress the patched production message-queue functions."""
from pathlib import Path
import subprocess
from os_queue_patch import patch_os_queues
root=Path(__file__).resolve().parents[2]
source=patch_os_queues((root/"upstream/goldeneye-pc/port/src/libultra.c").read_text())
def section(start,end):
    return source[source.index(start):source.index(end,source.index(start))]
fixture=(root/"native-engine/tests/os_queue_host.c").read_text()
functions=section("static PortQueue *portQueueGet(OSMesgQueue *mq)\n{","extern OSMesgQueue gfxFrameMsgQ;")
# The unchanged deterministic branch is outside this queue concurrency test.
recv=section("s32 osRecvMesg(","/* ------------------------------------------------------------------------ */\n/* Events")
a=recv.index("        /* GE_DETERM");b=recv.index("        pthread_cond_wait",a)
recv=recv[:a]+recv[b:]
recv=recv.replace("    if (mq == &gfxFrameMsgQ) watchSettingsGameTick();","")
force=section("static void portPostEventForce(","\nstatic void portPostEvent(OSId e)")
fixture=fixture.replace("/* PRODUCTION_FUNCTIONS */",functions+recv+force)
out=root/"native-engine/build/os-queue-test"
out.parent.mkdir(parents=True,exist_ok=True)
c=out.with_suffix(".c");c.write_text(fixture)
subprocess.run(["cc","-std=gnu11","-O2","-Wall","-Wextra","-Werror","-pthread","-fsanitize=address,undefined","-I"+str(root/"native-engine/include"),str(c),"-o",str(out)],check=True)
subprocess.run([str(out)],check=True,timeout=30)
