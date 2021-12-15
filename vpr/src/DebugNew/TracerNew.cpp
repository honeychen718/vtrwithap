#include "TracerNew.h"

#ifndef NDEBUG

TracerNew NewTracer;

bool TracerNew::Ready = false;

void* operator new(size_t size, const char *file, long line){
    void* p = malloc(size);
    if(TracerNew::Ready)
        NewTracer.Add(p,file,line);
    return p;
}

void* operator new[](size_t size, const char *file, long line){
    void* p = malloc(size);
    if(TracerNew::Ready)
        NewTracer.Add(p,file,line);
    return p;
}

// void* operator new(size_t size){
//     void* p = malloc(size);
//     if(TracerNew::Ready)
//         NewTracer.Add(p,"Unknow",-1);
//         return p;
// }

// void* operator new[](size_t size){
//     void* p = malloc(size);
//     if(TracerNew::Ready)
//         NewTracer.Add(p,"Unknow",-1);
//         return p;
// }

void operator delete(void *p){
    if(TracerNew::Ready)
        NewTracer.Remove(p);
    free(p);
}

void operator delete[](void *p){
    if(TracerNew::Ready)
        NewTracer.Remove(p);
    free(p);
}

TracerNew::TracerNewInfo::TracerNewInfo(const char* file, long line):file_(file),line_(line){

}

const char* TracerNew::TracerNewInfo::File() const{
    return file_;
}

long TracerNew::TracerNewInfo::Line() const{
    return line_;
}


TracerNew::TracerNew(/* args */):lock_count_(0)
{
    TracerNew::Ready = true;
}

TracerNew::~TracerNew()
{
    TracerNew::Ready = false;
    Dump();
}

void TracerNew::Add(void* p, const char *file, long line){
    if(lock_count_>0)
        return;
    Lock lock(*this);
    tracer_infos_[p] = TracerNewInfo(file,line);
    //std::cout<<"add"<<std::endl;
}

void TracerNew::Remove(void* p){
    if(lock_count_>0)
        return;
    Lock lock(*this);
    auto it = tracer_infos_.find(p);
    if(it != tracer_infos_.end()){
        tracer_infos_.erase(it);
        //std::cout<<"remove"<<std::endl;
    }
}

void TracerNew::Dump(){
    for(auto tracer_info : tracer_infos_){
        std::cout<<"0x"<<tracer_info.first<<":\t"<<tracer_info.second.File()<<"\tIn_Line"<<tracer_info.second.Line()<<std::endl;
    }
}

#endif//!NDEBUG