#pragma once

// ��ѭ"��С��ԭ��"���Ƿ�ҳ�ڴ�Ƚϰ�������Ĭ���Ƿ�ҳ�ڴ�
void* _cdecl operator new(size_t size, POOL_TYPE type, ULONG tag = 0);

// size �Ǳ�����ȷ����
// placement new
void* _cdecl operator new(size_t size, void* p);

// https://stackoverflow.com/questions/10397826/overloading-operator-new-with-wdk
void _cdecl operator delete(void* p, size_t);

void _cdecl operator delete(void* p);

void* _cdecl operator new[](size_t size, POOL_TYPE type, ULONG tag);

void _cdecl operator delete[](void* p, size_t);
