#ifndef EOG_NATIVE_RETAINED_INDEX_H
#define EOG_NATIVE_RETAINED_INDEX_H
#include <cstdint>
/* Hash collisions must not evict geometry while another slot is free.
 * Buckets index a fixed pool; replacement follows one global LRU list. */
template<unsigned Capacity,unsigned Buckets> class NativeRetainedIndex {
    static_assert(Capacity<32767 && Buckets>0);
    int16_t buckets[Buckets],chain[Capacity],older[Capacity],newer[Capacity];
    uint32_t hashes[Capacity];
    unsigned used=0;int newest=-1,oldest=-1;
    void unlink(unsigned i) {
        int a=older[i],b=newer[i];
        if(a>=0)newer[a]=b;else oldest=b;
        if(b>=0)older[b]=a;else newest=a;
    }
public:
    NativeRetainedIndex(){clear();}
    void clear(){for(auto &b:buckets)b=-1;used=0;newest=oldest=-1;}
    template<class Matches> int find(uint32_t hash,Matches matches)const {
        for(int i=buckets[hash%Buckets];i>=0;i=chain[i])if(hashes[i]==hash && matches(unsigned(i)))return i;
        return -1;
    }
    unsigned victim()const{return used<Capacity?used:unsigned(oldest);}
    void touch(unsigned i) {
        if(int(i)==newest)return;
        unlink(i);older[i]=newest;newer[i]=-1;
        if(newest>=0)newer[newest]=i;else oldest=i;newest=i;
    }
    void install(unsigned i,uint32_t hash) {
        if(i<used) {
            int16_t *link=&buckets[hashes[i]%Buckets];
            while(*link!=int(i))link=&chain[*link];
            *link=chain[i];unlink(i);
        }else ++used;
        hashes[i]=hash;chain[i]=buckets[hash%Buckets];buckets[hash%Buckets]=i;
        older[i]=newest;newer[i]=-1;
        if(newest>=0)newer[newest]=i;else oldest=i;newest=i;
    }
};
#endif
