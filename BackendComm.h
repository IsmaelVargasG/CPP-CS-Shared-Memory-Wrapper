#ifndef BACKENDCOMM_H
#define BACKENDCOMM_H

#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>

using milliseconds = std::chrono::milliseconds;

template <typename T>
class BackendComm{
	public:
		// Creates a named file mapping object and its event (sender) with the default settings and saves the name of another mapping object (receiver).
		BackendComm(std::string senderName, std::string receiverName);


		///// Send Functions /////

		// Reinstantiate the sender.
		void SetSender();
		// Writes the given data into the file mapping object and signals an event.
		void Send(T& payload);
		
		// Changes the configuration of the sender's named file mapping view.
		void SetSenderViewConfig(DWORD accessMode, DWORD fileOffsetH, DWORD fileOffsetL, SIZE_T nBytes);
		// Changes the configuration of the sender's file mapping object.
		void SetSenderMapConfig(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes, DWORD flProtect, DWORD maxSizeH, DWORD maxSizeL);
		// Changes the configuration of the sender's named event.
		void SetSenderEventConfig(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL manualReset, BOOL initialState);


		///// Receive Functions /////

		// Tries to open a named file mapping object and its event (receiver) with the name set in the constructor until it succeeds or reaches the set timeout.
		bool TryConnect(milliseconds timeout = (milliseconds::max)(), milliseconds iterDelay = milliseconds(100));
		// Returns true if the receiver has successfully opened its named file mapping object.
		bool isConnected() { return this->_isConnected; }

		// Changes the configuration of the receiver's named file mapping view.
		void SetreceiverViewConfig(DWORD accessMode, DWORD fileOffsetH, DWORD fileOffsetL, SIZE_T nBytes);
		// Changes the configuration of the receiver's file mapping object.
		void SetreceiverMapConfig(DWORD fileMapAccessMode, BOOL fileMapInheritHandle);
		// Changes the configuration of the receiver's named event.
		void SetreceiverEventConfig(DWORD eventAccessMode, BOOL eventInheritHandle);

		// Returns a pointer to the data written in the file mapping object.
		T* ReceivePoint();
		// Waits until the receiver is in the signaled state.
		DWORD WaitObject(DWORD milliseconds = INFINITE);

		~BackendComm() = default;


	private:
		struct MapViewConfig {
			DWORD _accessMode{ FILE_MAP_READ | FILE_MAP_WRITE };
			DWORD _fileOffsetH{ 0 };
			DWORD _fileOffsetL{ 0 };
			SIZE_T _nBytes{ sizeof(T) };
		};

		// Sender
		struct Sender {
			HANDLE _hMapOut;
			HANDLE _hEventOut;
		};

		struct OutFileMapConfig {
			HANDLE _hFile{ INVALID_HANDLE_VALUE };
			LPSECURITY_ATTRIBUTES _lpFileMappingAttributes{ NULL };
			DWORD _flProtect{ PAGE_READWRITE };
			DWORD _maxSizeH{ 0 };
			DWORD _maxSizeL{ sizeof(T) };
		};

		struct OutEventConfig {
			LPSECURITY_ATTRIBUTES _lpEventAttributes{ NULL };
			BOOL _manualReset{ FALSE };
			BOOL _initialState{ FALSE };
		};

		struct SenderConfig {
			OutFileMapConfig _mapConfig;
			OutEventConfig _eventConfig;
		};

		// receiver
		struct receiver {
			HANDLE _hMapIn;
			HANDLE _hEventIn;
		};

		struct receiverConfig {
			DWORD _fileMapAccessMode{ FILE_MAP_READ };
			BOOL _fileMapInheritHandle{ FALSE };

			DWORD _eventAccessMode{ SYNCHRONIZE };
			BOOL _eventInheritHandle{ FALSE };
		};

		std::string _senderName;
		std::string _receiverName;

		MapViewConfig _mapViewConfS;
		MapViewConfig _mapViewConfR{ FILE_MAP_READ, 0, 0, sizeof(T) };

		Sender _sender;
		SenderConfig _senderConfig;

		receiver _receiver;
		receiverConfig _receiverConfig;

		bool _isConnected = false;
};



template <typename T>
BackendComm<T>::BackendComm(std::string senderName, std::string receiverName) : _senderName{ senderName }, _receiverName{ receiverName } {
	SetSender();
}

template <typename T>
void BackendComm<T>::SetSender(){
	BackendComm<T>::OutFileMapConfig mapConf = _senderConfig._mapConfig;
	BackendComm<T>::OutEventConfig eventConf = _senderConfig._eventConfig;

	std::string outMapName = "Global\\Map" + _senderName;
	std::string outEventName = "Global\\Event" + _senderName;

	_sender._hMapOut = CreateFileMappingA(mapConf._hFile, mapConf._lpFileMappingAttributes, mapConf._flProtect, mapConf._maxSizeH, mapConf._maxSizeL, outMapName.c_str());
	_sender._hEventOut = CreateEventA(eventConf._lpEventAttributes, eventConf._manualReset, eventConf._initialState, outEventName.c_str());
}

template <typename T>
void BackendComm<T>::Send(T& payload) {
	T* outData = (T*)MapViewOfFile(_sender._hMapOut, _mapViewConfS._accessMode, _mapViewConfS._fileOffsetH, _mapViewConfS._fileOffsetL, _mapViewConfS._nBytes);

	if (outData != nullptr) {
		std::memcpy(outData, &payload, sizeof(T));

		SetEvent(_sender._hEventOut);
	}
}

template <typename T>
void BackendComm<T>::SetSenderViewConfig(DWORD accessMode, DWORD fileOffsetH, DWORD fileOffsetL, SIZE_T nBytes) {
	_mapViewConfS = { accessMode, fileOffsetH, fileOffsetL, nBytes };
}

template <typename T>
void BackendComm<T>::SetSenderMapConfig(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes, DWORD flProtect, DWORD maxSizeH, DWORD maxSizeL) {
	_senderConfig._mapConfig = { hFile, lpFileMappingAttributes, flProtect, maxSizeH, maxSizeL };
}

template <typename T>
void BackendComm<T>::SetSenderEventConfig(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL manualReset, BOOL initialState) {
	_senderConfig._eventConfig = { lpEventAttributes, manualReset, initialState };
}

template <typename T>
bool BackendComm<T>::TryConnect(milliseconds timeout, milliseconds iterDelay) {
	std::string inMapName = "Global\\Map" + _receiverName;
	std::string inEventName = "Global\\Event" + _receiverName;

	auto start = std::chrono::steady_clock::now();
	auto deadline = start + std::chrono::duration<double, std::milli>(timeout);

	while (std::chrono::steady_clock::now() < deadline) {
		_receiver._hMapIn = OpenFileMappingA(_receiverConfig._fileMapAccessMode, _receiverConfig._fileMapInheritHandle, inMapName.c_str());
		_receiver._hEventIn = OpenEventA(_receiverConfig._eventAccessMode, _receiverConfig._eventInheritHandle, inEventName.c_str());

		if (_receiver._hMapIn == nullptr || _receiver._hEventIn == nullptr) {
			std::cout << "Conectando...\n";
			std::this_thread::sleep_for(iterDelay);
		}
		else {
			std::cout << "Conexion establecida.\n";
			_isConnected = true;

			return true;
		}
	}

	return false;
}

template <typename T>
T* BackendComm<T>::ReceivePoint() {
	T* inData = (T*)MapViewOfFile(_receiver._hMapIn, _mapViewConfR._accessMode, _mapViewConfR._fileOffsetH, _mapViewConfR._fileOffsetL, _mapViewConfR._nBytes);

	return inData;
}

template <typename T>
DWORD BackendComm<T>::WaitObject(DWORD milliseconds) {
	return WaitForSingleObject(_receiver._hEventIn, INFINITE);
}

template <typename T>
void BackendComm<T>::SetreceiverViewConfig(DWORD accessMode, DWORD fileOffsetH, DWORD fileOffsetL, SIZE_T nBytes){
	_mapViewConfR = { accessMode, fileOffsetH, fileOffsetL, nBytes };
}

template <typename T>
void BackendComm<T>::SetreceiverMapConfig(DWORD fileMapAccessMode, BOOL fileMapInheritHandle) {
	_receiverConfig._fileMapAccessMode = fileMapAccessMode;
	_receiverConfig._fileMapInheritHandle = fileMapInheritHandle;
}

template <typename T>
void BackendComm<T>::SetreceiverEventConfig(DWORD eventAccessMode, BOOL eventInheritHandle) {
	_receiverConfig._eventAccessMode = eventAccessMode;
	_receiverConfig._eventInheritHandle = eventInheritHandle;
}

#endif // !BACKENDCOMM_H