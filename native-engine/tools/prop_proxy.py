"""Host-only clustered distance silhouettes for selected rigid prop parts.

Each connected component keeps its own bounds and transform domain. Far and
very-far tiers deliberately sacrifice small surface detail, without changing
original vertices, UVs, collision or close-up meshes.
"""
import collections

def maps_for(name,points,triangles,protected):
    if name not in ('PmiltruckZ','Pboxes4x4Z') or protected:return None
    parent=list(range(len(points)))
    def root(i):
        while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
        return i
    for tri in triangles:
        for a,b in zip(tri,tri[1:]):parent[root(a)]=root(b)
    components=collections.defaultdict(list)
    for i in range(len(points)):components[root(i)].append(i)
    result=[]
    for cells in (3,2):
        mapping=list(range(len(points)))
        for ids in components.values():
            if len(ids)<8:continue
            low=[min(points[i][a] for i in ids) for a in range(3)]
            high=[max(points[i][a] for i in ids) for a in range(3)]
            buckets=collections.defaultdict(list)
            for i in ids:
                key=tuple(min(cells-1,(points[i][a]-low[a])*cells//max(1,high[a]-low[a])) for a in range(3))
                buckets[key].append(i)
            for key,group in buckets.items():
                # Preserve extreme corners instead of shrinking each cluster
                # toward its average. Representatives are authored points.
                target=[low[a]+(high[a]-low[a])*key[a]/(cells-1) for a in range(3)]
                representative=min(group,key=lambda i:(sum(((points[i][a]-target[a])/max(1,high[a]-low[a]))**2 for a in range(3)),i))
                for i in group:mapping[i]=representative
        result.append(mapping)
    return result

def surviving(triangles,mapping,points):
    count=0
    for tri in triangles:
        a,b,c=[points[mapping[i]] for i in tri]
        ab=[b[i]-a[i] for i in range(3)];ac=[c[i]-a[i] for i in range(3)]
        if any((ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0])):count+=1
    return count
