using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Runtime.CompilerServices;
using System.Text;

namespace Frontend
{
    public class FrontendComm<T> where T: unmanaged
    {
        #region Public Members

        /// <summary>
        /// Creates a <see cref="Sender"/> object with the default settings and saves the name for the <see cref="Receiver"/> object.
        /// </summary>
        /// <param name="senderName"></param>
        /// <param name="receiverName"></param>
        /// <exception cref="InvalidOperationException"></exception>
        public FrontendComm(string senderName, string receiverName)
        {
            _senderName = senderName;
            _receiverName = receiverName;

            if (!IsUnmanaged())
            {
                throw new InvalidOperationException(
                    $"El tipo {typeof(T).Name} debe ser unmanaged (sin referencias)");
            }

            SetSender();
        }


        ///// Send Functions /////

        /// <summary>
        /// Reinstantiate the <see cref="Sender"/>.
        /// </summary>
        public void SetSender()
        {
            string outMapName = "Global\\Map" + _senderName;
            string outEventName = "Global\\Event" + _senderName;

            _sender._mapFileOut = MemoryMappedFile.CreateNew(outMapName, Unsafe.SizeOf<T>());
            _sender._eventOut = new EventWaitHandle(_senderConfig._initialState, _senderConfig._resetMode, outEventName);
            _sender._accessorOut = _sender._mapFileOut.CreateViewAccessor(_senderViewConfig._offset, Unsafe.SizeOf<T>(), _senderViewConfig._access);
        }

        /// <summary>
        /// Writes the given data into the <see cref="Sender"/> and signals an event.
        /// </summary>
        /// <param name="payload"></param>
        public void Send(ref T payload)
        {
            _sender._accessorOut.Write(0, ref payload);

            _sender._eventOut.Set();
        }



        ///// Receive Functions /////

        /// <summary>
        /// Tries to open the <see cref="Receiver"/> using the name given in the constructor until it succeeds or reaches the set timeout.
        /// </summary>
        /// <param name="timeout"></param>
        /// <param name="iterDelay"></param>
        /// <returns><see langword="true"/> if the <see cref="Receiver"/> was successfully opened; otherwise, <see langword="false"/>.</returns>
        public bool TryConnect(long timeout = Int64.MaxValue, int iterDelay = 100)
        {
            string InMapName = "Global\\Map" + _receiverName;
            string InEventName = "Global\\Event" + _receiverName;

            Stopwatch stopwatch = Stopwatch.StartNew();

            while(stopwatch.ElapsedMilliseconds < timeout)
            {
                try
                {
                    _receiver._mapFileIn = MemoryMappedFile.OpenExisting(InMapName);
                    _receiver._eventIn = EventWaitHandle.OpenExisting(InEventName);
                    _receiver._accessorIn = _receiver._mapFileIn.CreateViewAccessor(_receiverViewConfig._offset, Unsafe.SizeOf<T>(), _receiverViewConfig._access);

                    Console.WriteLine("Conexion establecida");
                    _isConnected = true;
                    return true;
                }
                catch (FileNotFoundException)
                {
                    Console.WriteLine("Conectando...");
                    Thread.Sleep(iterDelay);
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Error inesperado al conectar: {ex.Message}");
                    throw;
                }
            }

            Console.WriteLine($"Timeout alcanzado después de {stopwatch.ElapsedMilliseconds}ms");
            return false;
        }

        /// <summary>
        /// Waits until an event is signaled or it reaches the set timeout. If an event is signaled, it reads a structure of type <typeparamref name="T"/>.
        /// </summary>
        /// <param name="timeout"></param>
        /// <returns>The read structure</returns>
        public T Receive(int timeout = Int32.MaxValue)
        {
            _receiver._eventIn.WaitOne(timeout);
            _receiver._accessorIn.Read(0, out T payload);

            return payload;
        }

        #endregion



        #region Private Members

        /// <summary>
        /// Checks if the data type used for the constructor is an unmanaged type.
        /// </summary>
        /// <returns><see langword="true"/> if unmanaged; otherwise, <see langword="false"/></returns>
        private static bool IsUnmanaged()
        {
            Type type = typeof(T);

            if(type.IsPrimitive || type.IsEnum)
                return true;

            foreach(var field in type.GetFields(
                System.Reflection.BindingFlags.Instance |
                System.Reflection.BindingFlags.Public |
                System.Reflection.BindingFlags.NonPublic))
            {
                Type fieldType = field.FieldType;

                if (!fieldType.IsValueType)
                    return false;
            }

            return true;
        }

        private struct MapViewConfig
        {
            public long _offset;
            public MemoryMappedFileAccess _access;

            public MapViewConfig(long offset = 0, MemoryMappedFileAccess access = MemoryMappedFileAccess.Write)
            {
                _offset = offset;
                _access = access;
            }
        }

        private struct Sender
        {
            public MemoryMappedFile _mapFileOut;
            public EventWaitHandle _eventOut;
            public MemoryMappedViewAccessor _accessorOut;
        }

        private struct SenderConfig
        {
            public bool _initialState;
            public EventResetMode _resetMode;

            public SenderConfig(bool initialState = false, EventResetMode resetMode = EventResetMode.AutoReset)
            {
                _initialState = initialState;
                _resetMode = resetMode;
            }
        }

        private struct Receiver
        {
            public MemoryMappedFile _mapFileIn;
            public EventWaitHandle _eventIn;
            public MemoryMappedViewAccessor _accessorIn;
        }

        private readonly string _senderName;
        private readonly string _receiverName;
        private readonly long size;

        private Sender _sender;
        private SenderConfig _senderConfig = new();
        private MapViewConfig _senderViewConfig = new();

        private Receiver _receiver;
        private MapViewConfig _receiverViewConfig = new(0, MemoryMappedFileAccess.Read);

        private bool _isConnected { get; set; } = false;

        #endregion
    }
}
