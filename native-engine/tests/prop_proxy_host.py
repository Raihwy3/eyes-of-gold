"""Distance proxies retain transform/component isolation and close geometry."""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import prop_proxy

def main():
    # Two separate rigid pieces with different bounds must never weld together.
    points=[];triangles=[]
    for origin in (0,100):
        base=len(points)
        points.extend((origin+x,y,z) for x in range(4) for y in range(4) for z in range(4))
        # Connect a dense grid in each component, leaving no cross-piece edges.
        for i in range(base,base+62):triangles.append((i,i+1,i+2))
    original=list(points)
    maps=prop_proxy.maps_for('PmiltruckZ',points,triangles,set())
    assert maps and points==original
    for mapping in maps:
        assert all(0<=i<len(points) for i in mapping)
        assert all((i<64)==(j<64) for i,j in enumerate(mapping))
        for start in (0,64):
            assert len(set(mapping[start:start+64]))<64
            for axis in range(3):
                assert min(points[i][axis] for i in mapping[start:start+64])==min(p[axis] for p in points[start:start+64])
                assert max(points[i][axis] for i in mapping[start:start+64])==max(p[axis] for p in points[start:start+64])
    assert prop_proxy.maps_for('CguardZ',points,triangles,set()) is None
    assert prop_proxy.maps_for('PmiltruckZ',points,triangles,{0}) is None
    print('PASS prop proxy isolation: original points and component bounds preserved; cross-joint and non-whitelisted models excluded')
if __name__=='__main__':main()
