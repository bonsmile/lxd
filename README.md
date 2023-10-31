# C++ 工具集

### 时间和日期

c/c++ 表示时间的数据结构有：`time_t`, `tm`

time_t 是个长整型，表示从 1970.1.1 到某个时间的秒数

`tm` 是 c 里面常用表示日期的数据结构
```
struct tm{
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};
```

可以通过 `localtime_s/localtime_r` 将 `time_t` 转换成 `tm`; 通过 `mktime` 将 `tm` 转换成 `time_t`

chrono



time_t, chrono::system_clock

	const std::time_t t = std::time(nullptr); // usually has "1 second" precision
    const auto from = std::chrono::system_clock::from_time_t(t);

* Current timestamp in nanoseconds, milliseconds, seconds
* Date format: YYYYMMDD

### String

字符编码转换

* Convert std::wstring to std::string(UTF-8)
* Convert std::string(UTF-8) to std::wstring

字符串拼接

	std::string result = absl::StrCat("abc", "efg");

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

