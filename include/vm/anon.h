#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
#include "devices/disk.h"
struct page;
enum vm_type;

struct anon_page {
    // 디스크는 총 섹터 수 정해져 있음 (비트맵에 매핑 -> 비트맵 스캔하면 비어있는 섹터를 바로 찾을 수 있음 , 찾은 위치부터 비어있는데 채우고 )
    disk_sector_t start_sector_num;   // start부터 8개는 무조건 한 페이지꺼 (인덱스)
};


void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
