#ifndef TRACER_NEW_H_
#define TRACER_NEW_H_
#include <cstdlib>
#include <map>
#include <iostream>

#ifndef NDEBUG
void* operator new(size_t size, const char *file, long line);
void* operator new[](size_t size, const char *file, long line);
//void* operator new(size_t size);
// void* operator new[](size_t size);
void operator delete(void *p);
void operator delete[](void *p);


class TracerNew
{
    class TracerNewInfo{
    public:
        TracerNewInfo(const char *file = nullptr, long line = 0);
        const char* File() const;
        long Line() const;
    private:
        const char *file_;
        long line_;
    };

    class Lock{
    public:
        Lock(TracerNew &tracer):tracer_(tracer){
            tracer.lock_count_++;
        }
        ~Lock(){
            tracer_.lock_count_--;
        }
    private:
        TracerNew &tracer_;
    };
private:
    /* data */
    std::map<void*,TracerNewInfo> tracer_infos_;
    long lock_count_;
public:
    static bool Ready;
    TracerNew(/* args */);
    ~TracerNew();
    void Add(void* p, const char *file, long line);
    void Remove(void* p);
    void Dump();
};

extern TracerNew NewTracer;

#endif//!NDEBUG

#endif//!TRACER_NEW_H