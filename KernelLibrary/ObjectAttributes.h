#pragma once

enum class ObjectAttributesFlags {
	None = 0,
	KernelHandle = OBJ_KERNEL_HANDLE,// ��ʾʹ���ں˾���������������ϵͳ�����
	Caseinsensive = OBJ_CASE_INSENSITIVE,// ��ʾ�ں˶�������ֲ����ִ�Сд
	OpenIf = OBJ_OPENIF,
	ForeceAccessCheck = OBJ_FORCE_ACCESS_CHECK,
	Permanent = OBJ_PERMANENT,
	Exclusive = OBJ_EXCLUSIVE,
	Inherite = OBJ_INHERIT,	// ��ʾ�ں˶���ľ�����Ա��̳�
};
DEFINE_ENUM_FLAG_OPERATORS(ObjectAttributesFlags);

// ��������Ҫ�򿪻򴴽����ں˶�������
struct ObjectAttributes :OBJECT_ATTRIBUTES {
	// rootDirectory��ʾ����ĸ�Ŀ¼���
	// name ��ʾ�����·�������֣���rootDirectory��ͬ�����һ�������Ķ���ȫ·������
	// flags ��ʾ����򿪻򴴽�ʱ�ľ�������
	ObjectAttributes(PUNICODE_STRING name, ObjectAttributesFlags flags = ObjectAttributesFlags::None,
		HANDLE rootDirectory = nullptr, PSECURITY_DESCRIPTOR sd = nullptr);
};