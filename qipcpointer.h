#ifndef QIPCPOINTER_H
#define QIPCPOINTER_H

#include <QtCore/QSharedMemory>
#include <QtCore/QSharedPointer>

template <typename T>
class QIpcPointer
{
public:
	QIpcPointer();
	QIpcPointer(const QIpcPointer<T> &other) = default;
	QIpcPointer &operator=(const QIpcPointer &other) = default;
	QIpcPointer(QIpcPointer<T> &&other) noexcept = default;
	QIpcPointer& operator=(QIpcPointer<T> &&other) noexcept = default;
	~QIpcPointer() = default;

	template <typename... TArgs>
	static QIpcPointer<T> create(const QString &key, TArgs&&... args);
	static QIpcPointer<T> attach(const QString &key);

	QIpcPointer<T> clone(const QIpcPointer<T> &other) const;

	explicit operator bool() const;
	bool operator!() const;
	bool isValid() const;
	bool isNull() const;
	bool isOwner() const;

	QSharedMemory::SharedMemoryError error() const;
	QString errorString() const;

	QString key() const;
	T &operator*() const;
	T *operator->() const;
	T *data() const;
	T *get() const;

	void dropOwnership();
	void clear();

#ifndef QT_NO_SYSTEMSEMAPHORE
	bool lock() const;
	bool unlock() const;
#endif

	QSharedMemory *sharedMemory() const;

	friend void swap(QIpcPointer<T> &lhs, QIpcPointer<T> &rhs) noexcept;

protected:
	struct Ptr {
		bool owned = true;
		quint64 count = 1;
		T data;

		template<typename... TArgs>
		Ptr(TArgs&&... args);
	};
	struct Data {
		Q_DISABLE_COPY(Data)

		QSharedMemory sharedMem;
		bool isOwner = false;
		Ptr *data = nullptr;
		QSharedMemory::SharedMemoryError errorOverride = QSharedMemory::NoError;

		~Data();
	};
	QSharedPointer<Data> d;
};

class QIpcPointerLocker
{
	Q_DISABLE_COPY(QIpcPointerLocker)
public:
	template <typename T>
	QIpcPointerLocker(const QIpcPointer<T> &pointer);
	QIpcPointerLocker(QIpcPointerLocker &&other) noexcept = default;
	QIpcPointerLocker& operator=(QIpcPointerLocker &&other) noexcept = default;
	inline ~QIpcPointerLocker();

	inline bool unlock();
	inline bool relock();

private:
	QSharedMemory *_shareMem;
	bool _locked = false;
};

Q_DECLARE_SMART_POINTER_METATYPE(QIpcPointer)



template<typename T>
QIpcPointer<T>::QIpcPointer() :
	d{new Data{}}
{}

template<typename T>
template<typename... TArgs>
QIpcPointer<T> QIpcPointer<T>::create(const QString &key, TArgs&&... args)
{
	QIpcPointer<T> self;
	self.d->sharedMem.setKey(key);
	if(!self.d->sharedMem.create(sizeof(Ptr), QSharedMemory::ReadWrite))
		return self;
	Q_ASSERT_X(self.d->sharedMem.size() >= static_cast<int>(sizeof(Ptr)), Q_FUNC_INFO, "less memory returned than requested");

	self.d->isOwner = true;
	self.d->data = new (self.d->sharedMem.data()) Ptr{std::forward<TArgs>(args)...};
	return self;
}

template<typename T>
QIpcPointer<T> QIpcPointer<T>::attach(const QString &key)
{
	QIpcPointer<T> self;
	self.d->sharedMem.setKey(key);
	if(!self.d->sharedMem.attach(QSharedMemory::ReadWrite))
		return self;
	if(self.d->sharedMem.size() < static_cast<int>(sizeof(Ptr))) {
		self.d->errorOverride = QSharedMemory::InvalidSize;
		return self;
	}

	self.lock();
	self.d->data = reinterpret_cast<Ptr*>(self.d->sharedMem.data());
	self.d->data->count++;
	self.unlock();
	return self;
}

template<typename T>
QIpcPointer<T> QIpcPointer<T>::clone(const QIpcPointer<T> &other) const
{
	if(other)
		return QIpcPointer<T>::attach(other.key());
	else
		return {};
}

template<typename T>
QIpcPointer<T>::operator bool() const
{
	return d->data;
}

template<typename T>
bool QIpcPointer<T>::operator!() const
{
	return !d->data;
}

template<typename T>
bool QIpcPointer<T>::isValid() const
{
	return d->data;
}

template<typename T>
bool QIpcPointer<T>::isNull() const
{
	return !d->data;
}

template<typename T>
bool QIpcPointer<T>::isOwner() const
{
	return d->isOwner;
}

template<typename T>
QSharedMemory::SharedMemoryError QIpcPointer<T>::error() const
{
	if(d->errorOverride != QSharedMemory::NoError)
		return d->errorOverride;
	else
		return d->sharedMem.error();
}

template<typename T>
QString QIpcPointer<T>::errorString() const
{
	if(d->errorOverride != QSharedMemory::NoError) {
		return QStringLiteral("Was able to attach to shared memory, "
							  "but the attached memory only provides %L1 bytes, "
							  "whilst for the given datatype (+ metadata) %L2 bytes are required")
				.arg(d->sharedMem.size())
				.arg(sizeof(Ptr));
	} else
		return d->sharedMem.error();
}

template<typename T>
QString QIpcPointer<T>::key() const
{
	return d->sharedMem.key();
}

template<typename T>
T &QIpcPointer<T>::operator*() const
{
	return d->data->data;
}

template<typename T>
T *QIpcPointer<T>::operator->() const
{
	return &(d->data->data);
}

template<typename T>
T *QIpcPointer<T>::data() const
{
	return d->data ? &(d->data->data) : nullptr;
}

template<typename T>
T *QIpcPointer<T>::get() const
{
	return d->data ? &(d->data->data) : nullptr;
}

template<typename T>
void QIpcPointer<T>::dropOwnership()
{
	if(d->isOwner && d->data) {
		lock();
		d->data->owned = false;
		d->isOwner = false;
		unlock();
	}
}

template<typename T>
void QIpcPointer<T>::clear()
{
	d.clear();
}

#ifndef QT_NO_SYSTEMSEMAPHORE
template<typename T>
bool QIpcPointer<T>::lock() const
{
	return d->sharedMem.lock();
}

template<typename T>
bool QIpcPointer<T>::unlock() const
{
	return d->sharedMem.unlock();
}
#endif

template<typename T>
QSharedMemory *QIpcPointer<T>::sharedMemory() const
{
	return &(d->sharedMem);
}

template<typename T>
template<typename... TArgs>
QIpcPointer<T>::Ptr::Ptr(TArgs&&... args) :
	data{std::forward<TArgs>(args)...}
{}

template<typename T>
QIpcPointer<T>::Data::~Data()
{
	const auto isAttached = sharedMem.isAttached();

	if(data && (isOwner || !data->owned)) {
		if(isAttached)
			sharedMem.lock();
		if(isOwner || --(data->count) == 0)
			data->~Ptr();
		if(isAttached)
			sharedMem.unlock();
	}

	if(isAttached)
		sharedMem.detach();
}

template<typename T>
void swap(QIpcPointer<T> &lhs, QIpcPointer<T> &rhs) noexcept
{
	std::swap(lhs.d, rhs.d);
}



template<typename T>
QIpcPointerLocker::QIpcPointerLocker(const QIpcPointer<T> &pointer) :
	_shareMem{pointer.sharedMemory()}
{
	_locked = _shareMem->lock();
}

QIpcPointerLocker::~QIpcPointerLocker()
{
	if(_shareMem)
		_shareMem->unlock();
}

bool QIpcPointerLocker::unlock()
{
	if(_shareMem) {
		_locked = false;
		return _shareMem->unlock();
	} else
		return false;
}

bool QIpcPointerLocker::relock()
{
	if(_shareMem && !_locked && _shareMem->lock()) {
		_locked = true;
		return true;
	} else
		return false;
}

#endif // QIPCPOINTER_H
