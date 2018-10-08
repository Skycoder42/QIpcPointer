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
	static QIpcPointer<T> attach(const QString &key, QSharedMemory::AccessMode mode = QSharedMemory::ReadWrite);

	QIpcPointer<T> clone(const QIpcPointer<T> &other, QSharedMemory::AccessMode mode = QSharedMemory::ReadWrite) const;

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

	void clear();

#ifndef QT_NO_SYSTEMSEMAPHORE
	bool lock() const;
	bool unlock() const;
#endif

	QSharedMemory *sharedMemory() const;

	friend void swap(QIpcPointer<T> &lhs, QIpcPointer<T> &rhs) noexcept;

private:
	struct Data {
		QSharedMemory sharedMem;
		bool isOwner = false;
		T *data = nullptr;
		QSharedMemory::SharedMemoryError errorOverride = QSharedMemory::NoError;

		~Data();
	};
	QSharedPointer<Data> d;
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
	if(!self.d->sharedMem.create(sizeof(T), QSharedMemory::ReadWrite))
		return self;
	Q_ASSERT_X(self.d->sharedMem.size() >= sizeof(T), Q_FUNC_INFO, "less memory returned than requested");

	self.d->isOwner = true;
	self.d->data = new (self.d->sharedMem.data()) T{std::forward<TArgs>(args)...};
	return self;
}

template<typename T>
QIpcPointer<T> QIpcPointer<T>::attach(const QString &key, QSharedMemory::AccessMode mode)
{
	QIpcPointer<T> self;
	self.d->sharedMem.setKey(key);
	if(!self.d->sharedMem.attach(mode))
		return self;
	if(self.d->sharedMem.size() < sizeof(T)) {
		self.d->errorOverride = QSharedMemory::InvalidSize;
		return self;
	}

	self.d->data = reinterpret_cast<T*>(self.d->sharedMem.data());
	return self;
}

template<typename T>
QIpcPointer<T> QIpcPointer<T>::clone(const QIpcPointer<T> &other, QSharedMemory::AccessMode mode) const
{
	if(other)
		return QIpcPointer<T>::attach(other.key(), mode);
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
							  "whilst for the given datatype %L2 bytes are required")
				.arg(d->sharedMem.size())
				.arg(sizeof(T));
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
	return *(d->data);
}

template<typename T>
T *QIpcPointer<T>::operator->() const
{
	return d->data;
}

template<typename T>
T *QIpcPointer<T>::data() const
{
	return d->data;
}

template<typename T>
T *QIpcPointer<T>::get() const
{
	return d->data;
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
	return d->sharedMem;
}

template<typename T>
QIpcPointer<T>::Data::~Data()
{
	const auto isAttached = sharedMem.isAttached();

	if(isOwner && data) {
		if(isAttached)
			sharedMem.lock();
		data->~T();
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

#endif // QIPCPOINTER_H
