# C++ 工具集

### Timing

* Current timestamp in nanoseconds, milliseconds, seconds
* Date format: YYYYMMDD

### 字符编码

* Convert std::wstring to std::string(UTF-8)
* Convert std::string(UTF-8) to std::wstring

### File IO

* Open and Close File
* Read file into std::string
* Write bytes into file
* Create/Remove Dir
* Check if Dir exists

### Containers

* SmallVector: dynamic array on stack

### Http

* 同步



* 异步

### 多线程

互斥 ksMutex

信号 ksSignal

线程 ksThread

	// 创建线程，刚创建完处于挂起状态
	void Backup(void*) {}
	ksThread backupThread;
	ksThread_Create(&backupThread, "Backup", Backup, nullptr);
	// 激发线程，在线程执行过程中激发多次不会导致线程执行多次
	// 线程执行完又会进入挂起状态直到下次被激发
	ksThread_Signal(&backupThread);
	// 

线程池 ksThreadPool

	// 创建线程池
	ksThreadPool threadPool;
	ksThreadPool_Create( &threadPool, 4 );
	// 向线程池中提交任务
	struct MyData{} data;
	void WorkingThread(MyData* data);
	ksThreadPool_Submit( &threadPool, (ksThreadFunction)WorkingThread, &data );
	// 启动
	ksThreadPool_Join( &threadPool );
	// 销毁线程池
	ksThreadPool_Destroy( &threadPool );

### JSON

### glTF/glb

