#include "SystemTask.hpp"
#include "Observer.hpp"
#include "Subject.hpp" // TODO ---


//#include "bleApp.hpp"
//#include "BleUartService.hpp"
//static uint8_t ringBuffer[8] = {QueueEmpty};		// todo

/* todo -
 - what to do in enque() once queue == full?
    - override the data or wait till some vacancy?
*/
//SystemTask::SystemTask(Uart &pUart, MCP9808 &tmpSensor, NotificationManager &notificationManager, QueueHandle_t &systemQueue, BleUartService &bleService) : 
//		   uart(pUart), _tmpSensor(tmpSensor), _notificationManager(notificationManager), systemTaskQueue(systemQueue), _bleService(bleService)

SystemTask *_systemTask;

SystemTask::SystemTask(Uart &pUart, MCP9808 &tmpSensor, NotificationManager &notificationManager, QueueHandle_t &systemQueue) : 
		   uart(pUart), _tmpSensor(tmpSensor), _notificationManager(notificationManager), systemTaskQueue(systemQueue)
{   
    size_t heapSize1 = xPortGetFreeHeapSize();
    _systemTask = this;

    // create a task
    if (xTaskCreate(SystemTask::process, "PROCESS", 100, this, 0, &taskHandle) != pdPASS)	  // TODO - think about stack size!
    {
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    } 
      
    size_t heapSize2 = xPortGetFreeHeapSize();
    size_t heapTaken = heapSize1 - heapSize2;

    // BLE advertising and configuration
    //ble_stack_init();
    //gap_params_init();
    //gatt_init();
    //services_init();
    //advertising_init();
    //conn_params_init();

    // Start execution.
    //printf("\r\nUART started.\r\n");
    //NRF_LOG_INFO("Debug logging for UART over RTT started.");
    //advertising_start();
}

void SystemTask::process(void *instance)
{
    auto pInstance = static_cast<SystemTask*>(instance);
    pInstance->mainThread();
}

static uint8_t temp[40]; // TODO - remove
static uint16_t xaf = 0;
static int count = 0;

void CustSrvDataHdlr(CustEvent *bleCustEvent)
{
    uint8_t rxdBytes[100] = {0};
    SystemTask::Messages systemMsg;

    for (uint8_t idx = 0; idx < bleCustEvent->rxData.rxdBytes; idx++)
    {
        rxdBytes[idx] = bleCustEvent->rxData.rxBuffer[idx];
    }
        
    uint8_t const *rxData = bleCustEvent->rxData.rxBuffer;
    //char const val = (char ) *rxData;
    
    // get an appropriate message out of the requested bytes
    systemMsg = convertParsedInputToMsg((char *) rxdBytes);

    // route the corresponding message to the system task to take an apt action
    _systemTask->pushMessage(systemMsg, true);
}

/**
@brief: main state machine that handles requests from the user 
@description: current supported requests: enableNotifications(<typeOfNotif>), disableNotifications(<typeOfNotif>), 
	    where <typeOfNotif> corresponds to any supported sensor
*/
void SystemTask::mainThread()
{   
    Messages curMsg;

    //_tmpSensor.xferData(temp, 1);
    //_tmpSensor.read();
    // TODO - init peripherals?
    
        count++;
        //xaf = _tmpSensor.read();
        //vTaskDelay(pdMS_TO_TICKS(1000));
    while(true)
    {
        if (xQueueReceive(systemTaskQueue, &msg, portMAX_DELAY) == pdPASS)      // wait for the user input over UART (for now!)
        {
	  curMsg = static_cast<Messages>(msg);		         // TODO - might as well make msg type --> Message
	  Lookup lookupVal = lookupTable[msg];
	  uart.PrintUart("Message received: %s", lookupVal.name); // Messages::subscribeTempNotifications]);
	  //NRF_LOG_INFO("[SystemTask] -- message received: %d\n", curMsg);
	  	       
	  switch(curMsg)
	  {
	      case Messages::subscribeTempNotifications:
		_notificationManager.subscribe(&_tmpSensor);
		break;
	      
	      case Messages::unsubscribeTempNotifications:
		_notificationManager.unsubscribe(&_tmpSensor);
		break;
	      
	      default:
		break;
	  }
        }
    }
}

SystemTask::Messages convertParsedInputToMsg(const char *inputToCmp)
{
    if (!strcmp(inputToCmp, "tempOn"))
    {
        return SystemTask::Messages::subscribeTempNotifications;
    }
    else if (!strcmp(inputToCmp, "tempOff"))
    {
        return SystemTask::Messages::unsubscribeTempNotifications;
    }
    return SystemTask::Messages::invalidInput;
}

void SystemTask::pushMessage(SystemTask::Messages dataToQueue, bool fromISR)
{
    //NRF_LOG_WARNING("SystemTask::pushMessage()...\n");
    //NRF_LOG_FLUSH();
    BaseType_t xHigherPriorityTaskWoken = pdFALSE; // TODO - need?

    if (fromISR)
    {
        xQueueSendFromISR(systemTaskQueue, &dataToQueue, &xHigherPriorityTaskWoken);
    }
    else
    {
        xQueueSend(systemTaskQueue, &dataToQueue, 0);
    }

    //portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

}



