#pragma once

// ������IRQL��������Dispatch Level,�����IRQL,��ǰcpu��ռ�ã����ܵ��������߳�
// ��ϵͳ���ܵ�Ӱ���Ǿ޴�ģ�Ӧ���ݲ�ͬ��IRQL,ѡ����õ�ͬ��������������С��������
struct SpinLock {
public:
	void Init();

	void Lock();
	void Unlock();

private:
	KSPIN_LOCK _lock;
	KIRQL _oldIrql;
};