 /******************************************************************************
 *
 * Module: Port
 *
 * File Name: Port.c
 *
 * Description: Source file for TM4C123GH6PM Microcontroller - Port Driver.
 *
 * Author: Meriam Ehab
 ******************************************************************************/

#include "Port.h"
#include "tm4c123gh6pm_registers.h"

#include "Port_Regs.h"

#if (PORT_DEV_ERROR_DETECT == STD_ON)

#include "Det.h"
/* AUTOSAR Version checking between Det and Dio Modules */
#if ((DET_AR_MAJOR_VERSION != PORT_AR_RELEASE_MAJOR_VERSION)\
 || (DET_AR_MINOR_VERSION != PORT_AR_RELEASE_MINOR_VERSION)\
 || (DET_AR_PATCH_VERSION != PORT_AR_RELEASE_PATCH_VERSION))
  #error "The AR version of Det.h does not match the expected version"
#endif

#endif
   
 /*Static global variable to hold the status of 
  *the port module so that other API's can not 
  *be called if the Port module was uninitialized*/
 STATIC uint8 Port_Status = PORT_NOT_INITIALIZED;

  /*Static global variable to hold the status of the JTAG
   *pins to secure them and prevent the access to them*/
  STATIC uint8 JTAG_flag = STD_LOW;

 /* Holds the pointer of the Port_PinConfig */
 STATIC const Port_ConfigType* Port_ConfigPtr = NULL_PTR;


/************************************************************************************
* Service Name: Port_Init
* Service ID[hex]: 0x00
* Sync/Async:   Synchronous
* Reentrancy:   Non Reentrant
* Parameters (in)   :     ConfigPtr->Pointer to configuration set.
* Parameters (inout):     None
* Parameters (out)  :     None
* Return value:           None
* Description: Function to initialize the Port Driver module:
*              - Initializes ALL ports and port pins with the configuration set pointed to by the parameter ConfigPtr
*              - If Port_Init function is not called first, then no operation can occur on the MCU ports and port pins.
*              - Initializes all configured resources.
************************************************************************************/
void Port_Init(const Port_ConfigType* ConfigPtr)
{
  #if (PORT_DEV_ERROR_DETECT == STD_ON)
	/* check if the input configuration pointer is not a NULL_PTR */
	if (NULL_PTR == ConfigPtr)
	{
		Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, Port_Init_SID,
		     PORT_E_PARAM_CONFIG);
	}
	else
        { /*do nothing*/ }
  #endif
        
  Port_Status = PORT_INITIALIZED;
  Port_ConfigPtr = ConfigPtr;
  
  volatile uint32 *PortGpio_Ptr = NULL_PTR; /* Pointer to point to the required Port Registers base address */
  volatile uint32 delay = 0;
  
  for (Port_PinType index = 0; index < PORT_CONFIGURED_PINS; index++)
  {
    switch (Port_ConfigPtr-> Pins[index].Port_Num)
    {
      case 0: 
        PortGpio_Ptr = (volatile uint32 *)GPIO_PORTA_BASE_ADDRESS; /* PORTA Base Address */
	break;
      case 1: 
        PortGpio_Ptr = (volatile uint32 *)GPIO_PORTB_BASE_ADDRESS; /* PORTB Base Address */
	break;
      case 2: 
        PortGpio_Ptr = (volatile uint32 *)GPIO_PORTC_BASE_ADDRESS; /* PORTC Base Address */
	break;
      case 3: 
        PortGpio_Ptr = (volatile uint32 *)GPIO_PORTD_BASE_ADDRESS; /* PORTD Base Address */
	break;
      case 4: 
        PortGpio_Ptr = (volatile uint32 *)GPIO_PORTE_BASE_ADDRESS; /* PORTE Base Address */
	break;
      case 5: 
        PortGpio_Ptr = (volatile uint32 *)GPIO_PORTF_BASE_ADDRESS; /* PORTF Base Address */
	break;
    }
    
    /* Enable clock for PORT and allow time for clock to start*/
    SYSCTL_REGCGC2_REG |= (1<< (Port_ConfigPtr->Pins[index].Port_Num));
    delay = SYSCTL_REGCGC2_REG;
    
    
     /*Unlocking locked pins to be able to configure and use them*/
    if( ((Port_ConfigPtr->Pins[index].Port_Num == PORT_D) && (Port_ConfigPtr->Pins[index].Port_Num == NMI_PIN2)) || ((Port_ConfigPtr->Pins[index].Port_Num == PORT_F) && (Port_ConfigPtr->Pins[index].Port_Num == NMI_PIN1)) ) /* PD7 or PF0 */
    {
      /* Unlock the GPIOCR register */  
      *(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_LOCK_REG_OFFSET) = UNLOCK_VALUE; 
      
      /* Set the corresponding bit in GPIOCR register to allow changes on this pin */
      SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_COMMIT_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num);      
    }
    
    else if(  
            ( (Port_ConfigPtr->Pins[index].Port_Num == PORT_C) && (Port_ConfigPtr->Pins[index].Pin_Num == JTAG_PIN1) )||
            ( (Port_ConfigPtr->Pins[index].Port_Num == PORT_C) && (Port_ConfigPtr->Pins[index].Pin_Num == JTAG_PIN2) )||
            ( (Port_ConfigPtr->Pins[index].Port_Num == PORT_C) && (Port_ConfigPtr->Pins[index].Pin_Num == JTAG_PIN3) )||
            ( (Port_ConfigPtr->Pins[index].Port_Num == PORT_C) && (Port_ConfigPtr->Pins[index].Pin_Num == JTAG_PIN4) ))
    {
        /* Do Nothing ...  this is the JTAG pins */
        
        JTAG_flag = STD_HIGH;
    }
    else
    {
        /* Do Nothing ... No need to unlock the commit register for this pin */
    }
    
    if(Port_ConfigPtr->Pins[index].Pin_Mode == PORT_DIO_MODE)
    {
        /* Clear the corresponding bit in the GPIOAMSEL register to disable analog functionality on this pin */
	CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ANALOG_MODE_SEL_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num);

	/* Disable Alternative function for this pin by clear the corresponding bit in GPIOAFSEL register */
	CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ALT_FUNC_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num);

	/* Clear the PMCx bits for this pin */
	*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_CTL_REG_OFFSET) &= ~(0x0000000F << (Port_ConfigPtr->Pins[index].Pin_Num * 4));
        
        /*If output pin*/
        if(Port_ConfigPtr->Pins[index].Pin_Direction == PORT_PIN_OUT)
         {
           /* Set the corresponding bit in the GPIODIR register to configure it as output pin */
           SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIR_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num);                                    
            
           if(Port_ConfigPtr->Pins[index].Pin_initial_value== STD_HIGH)
            {
              /* Set the corresponding bit in the GPIODATA register to provide initial value 1 */
              SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DATA_REG_OFFSET) ,Port_ConfigPtr->Pins[index].Pin_Num);          
            }
            else
            {
              /* Clear the corresponding bit in the GPIODATA register to provide initial value 0 */
              CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DATA_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num);                    
            }
          }
         /*If Input pin*/
         else if(Port_ConfigPtr->Pins[index].Pin_Direction == PORT_PIN_IN)
         {
            CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIR_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num); 
            
            if(Port_ConfigPtr->Pins[index].resistor == PULL_UP)
            {
              /* Set the corresponding bit in the GPIOPUR register to enable the internal pull up pin */
              SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_PULL_UP_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num);                   
            }
            
            else if(Port_ConfigPtr->Pins[index].resistor == PULL_DOWN)
            {
              /* Set the corresponding bit in the GPIOPDR register to enable the internal pull down pin */
              SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_PULL_DOWN_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num);                 
            }
            
            else
            {
              /* Clear the corresponding bit in the GPIOPUR register to disable the internal pull up pin */                   
              CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_PULL_UP_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num);                   
              
              /* Clear the corresponding bit in the GPIOPDR register to disable the internal pull down pin */                
              CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_PULL_DOWN_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num);               
            }
          }
          else
          {
            /* Do Nothing */
          }
        
        /* Set the corresponding bit in the GPIODEN register to enable digital functionality on this pin */
	SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIGITAL_ENABLE_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num);
    
    }
    
    /*Configure Analog mode used for ADC only*/
    else if(Port_ConfigPtr->Pins[index].Pin_Mode == PORT_ALTERNATE_FUNCTION_ADC_MODE)
    {
      /* Clear the corresponding bit in the GPIODEN register to disable digital functionality on this pin */
      CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIGITAL_ENABLE_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num);   
      
      if(Port_ConfigPtr->Pins[index].Pin_Direction== PORT_PIN_OUT)
      {
         /* Set the corresponding bit in the GPIODIR register to configure it as output pin */
        SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIR_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num);                     
      }
      else if(Port_ConfigPtr->Pins[index].Pin_Direction == PORT_PIN_IN)
      {
        /* clear the corresponding bit in the GPIODIR register to configure it as input pin */              
        SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIR_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num);                      
      }
      
     /* Set the corresponding bit in the GPIOAMSEL register to enable analog functionality on this pin */      
      SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ANALOG_MODE_SEL_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num);                 
     }

     /*If any other alternative functionality between DIO Id and ADC id*/
     else if((Port_ConfigPtr->Pins[index].Pin_Mode > PORT_DIO_MODE) && (Port_ConfigPtr->Pins[index].Pin_Mode < PORT_ALTERNATE_FUNCTION_ADC_MODE))
     {
        /* Clear the corresponding bit in the GPIOAMSEL register to disable analog functionality on this pin */
        CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ANALOG_MODE_SEL_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num);  
        
        /* enable Alternative function for this pin by Setting the corresponding bit in GPIOAFSEL register */
        SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ALT_FUNC_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num);
        
        /* Clear the PMCx bits for this pin */
        *(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_CTL_REG_OFFSET) &= ~(0x0000000F << ((Port_ConfigPtr->Pins[index].Pin_Num) * 4));
        
        /* Set the PMCx bits for this pin to the selected Alternate function in the configurations */
        *(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_CTL_REG_OFFSET) |= (Port_ConfigPtr->Pins[index].Pin_Mode<< ((Port_ConfigPtr->Pins[index].Pin_Num)* 4)); 
        
        /* Set the corresponding bit in the GPIODEN register to enable digital functionality on this pin */
        SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIGITAL_ENABLE_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num);                           
      }
  }
}
    
#if (Port_Set_Pin_Direction_Api == STD_ON)

/************************************************************************************
* Service Name: Port_SetPinDirection
* Service ID[hex]: 0x01
* Sync/Async:   Synchronous
* Reentrancy:   Reentrant
* Parameters (in):
                  -Pin -> Port Pin ID number.
                  -Direction -> Port Pin Direction
* Parameters (inout):   None
* Parameters (out):     None
* Return value:         None
* Description: Function to set the port pin direction:
*              - sets the port pin direction during runtime
************************************************************************************/

void Port_SetPinDirection( Port_PinType Pin, Port_PinDirectionType Direction )
{
   boolean error = FALSE;
   
   /* point to the required Port Registers base address */
   volatile uint32 * Port_directionPtr = NULL_PTR; 
  
#if (PORT_DEV_ERROR_DETECT == STD_ON)
  /*Check if the API service called prior to module initialization*/
	if (Port_Status == PORT_NOT_INITIALIZED)
	{
		Det_ReportError(PORT_MODULE_ID, 
                                PORT_INSTANCE_ID, 
                                Port_Set_Pin_Direction_SID,
                                PORT_E_UNINIT);
                error = TRUE;
	}
	else
	{
            /* Do nothing*/
	}
        
        /*Dev error for Incorrect Port Pin ID passed*/
	if (Pin >= PORT_CONFIGURED_PINS)
	{
		Det_ReportError(PORT_MODULE_ID, 
                                PORT_INSTANCE_ID, 
                                Port_Init_SID,
                                PORT_E_PARAM_PIN);
                error = TRUE;
	} 
        else
	{
          /* Do nothing*/
	}
        
        if(((*Port_ConfigPtr->Pins)[Pin]).Pin_direction_changeable == PIN_DIRECTION_CHANGEABLE_OFF)
        {
          Det_ReportError(PORT_MODULE_ID, 
                          PORT_INSTANCE_ID, 
                          Port_Init_SID,
                          PORT_E_DIRECTION_UNCHANGEABLE);
          error = TRUE;
        }
        else 
        {
          /* Do nothing*/
        }
#endif
        
  /* In-case there are no errors */
  if(FALSE == error)
  {
    Port_Status = PORT_INITIALIZED;
	
    switch(Port_ConfigPtr->Pins[Pin].Port_Num)
    {
        case  0: Port_directionPtr = (volatile uint32 *)GPIO_PORTA_BASE_ADDRESS; /* PORTA Base Address */
		 break; 
	case  1: Port_directionPtr = (volatile uint32 *)GPIO_PORTB_BASE_ADDRESS; /* PORTB Base Address */
		 break;
	case  2: Port_directionPtr = (volatile uint32 *)GPIO_PORTC_BASE_ADDRESS; /* PORTC Base Address */
		 break;
	case  3: Port_directionPtr = (volatile uint32 *)GPIO_PORTD_BASE_ADDRESS; /* PORTD Base Address */
		 break;
        case  4: Port_directionPtr = (volatile uint32 *)GPIO_PORTE_BASE_ADDRESS; /* PORTE Base Address */
		 break;
        case  5: Port_directionPtr = (volatile uint32 *)GPIO_PORTF_BASE_ADDRESS; /* PORTF Base Address */
		 break;
    }
    
    /*Preventing any actions to be done upon JTAG pins*/
    if(  
        ( (Port_ConfigPtr->Pins[Pin].Port_Num == PORT_C) && (Port_ConfigPtr->Pins[Pin].Pin_Num == JTAG_PIN1) )||
        ( (Port_ConfigPtr->Pins[Pin].Port_Num == PORT_C) && (Port_ConfigPtr->Pins[Pin].Pin_Num == JTAG_PIN2) )||
        ( (Port_ConfigPtr->Pins[Pin].Port_Num == PORT_C) && (Port_ConfigPtr->Pins[Pin].Pin_Num == JTAG_PIN3) )||
        ( (Port_ConfigPtr->Pins[Pin].Port_Num == PORT_C) && (Port_ConfigPtr->Pins[Pin].Pin_Num == JTAG_PIN4) ))
    {
        /* Do Nothing ...  this is the JTAG pins */
        JTAG_flag = STD_HIGH;
    }
    
    else
    {
      switch (Direction)
      {
        case PORT_PIN_IN: 
             CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)Port_directionPtr + PORT_DIR_REG_OFFSET) , Pin);
             break; 
            
        case PORT_PIN_OUT:
             SET_BIT(*(volatile uint32 *)((volatile uint8 *)Port_directionPtr + PORT_DIR_REG_OFFSET) , Pin); 
             break;
             
        default:
             /*do nothing*/
             break;
            
                }/*End of the switch..case*/
          }/*end of the else condition*/
      }/*end of the if(FALSE == error)*/
}

#endif
    
/************************************************************************************
* Service Name: Port_RefreshPortDirection
* Service ID[hex]: 0x02
* Sync/Async: Synchronous
* Reentrancy: Non reentrant
* Parameters (in): None
* Parameters (inout): None
* Parameters (out): None
* Return value: None
* Description: Refreshes port direction.
************************************************************************************/
void Port_RefreshPortDirection(void)
{
   #if (PORT_DEV_ERROR_DETECT == STD_ON)
       /* Check if the Driver is initialized before using this function */
	if(Port_Status == PORT_NOT_INITIALIZED)
	{
	    Det_ReportError(PORT_MODULE_ID, 
                            PORT_INSTANCE_ID, 
                            Port_Refresh_Port_Direction_SID, 
                            PORT_E_UNINIT);
	}
	else
	{	
           /* Do Nothing */	
        }
   #endif

   for(Port_PinType index = 0; index < PORT_CONFIGURED_PINS; index++)
   {
     /* point to the required Port Registers base address */
     volatile uint32 * PortGpio_Ptr = NULL_PTR; 
     
     switch(Port_ConfigPtr->Pins[index].Port_Num)
     {
	case  0: 
          PortGpio_Ptr = (volatile uint32 *)GPIO_PORTA_BASE_ADDRESS; /* PORTA Base Address */
          break;
	case  1: 
          PortGpio_Ptr = (volatile uint32 *)GPIO_PORTB_BASE_ADDRESS; /* PORTB Base Address */
          break;
	case  2: 
          PortGpio_Ptr = (volatile uint32 *)GPIO_PORTC_BASE_ADDRESS; /* PORTC Base Address */
          break;
	case  3: 
          PortGpio_Ptr = (volatile uint32 *)GPIO_PORTD_BASE_ADDRESS; /* PORTD Base Address */
          break;
	case  4: 
          PortGpio_Ptr = (volatile uint32 *)GPIO_PORTE_BASE_ADDRESS; /* PORTE Base Address */
          break;
	case  5: 
          PortGpio_Ptr = (volatile uint32 *)GPIO_PORTF_BASE_ADDRESS; /* PORTF Base Address */
          break;
     }
    /*Preventing any actions to be done upon JTAG pins*/
    if(  
       ( (Port_ConfigPtr->Pins[index].Port_Num == PORT_C) && (Port_ConfigPtr->Pins[index].Pin_Num == JTAG_PIN1) )||
       ( (Port_ConfigPtr->Pins[index].Port_Num == PORT_C) && (Port_ConfigPtr->Pins[index].Pin_Num == JTAG_PIN2) )||
       ( (Port_ConfigPtr->Pins[index].Port_Num == PORT_C) && (Port_ConfigPtr->Pins[index].Pin_Num == JTAG_PIN3) )||
       ( (Port_ConfigPtr->Pins[index].Port_Num == PORT_C) && (Port_ConfigPtr->Pins[index].Pin_Num == JTAG_PIN4) ))
    {
        /* Do Nothing ...  this is the JTAG pins */
        JTAG_flag = STD_HIGH;
    }
    else 
    {
    }
    
    if (JTAG_flag == STD_LOW)
    {
      if (Port_ConfigPtr->Pins[index].Pin_direction_change == STD_OFF)
      {
        if(Port_ConfigPtr->Pins[index].Pin_Direction == PORT_PIN_OUT)
        {
          /* Set the corresponding bit in the GPIODIR register to configure it as output pin */
          SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIR_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num);
        }
        else if(Port_ConfigPtr->Pins[index].Pin_Direction == PORT_PIN_IN)
        {
          /* Clear the corresponding bit in the GPIODIR register to configure it as input pin */
          CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIR_REG_OFFSET) , Port_ConfigPtr->Pins[index].Pin_Num);
        }
        else
        {	
            /* Do Nothing */	
        }
      }
    }
    else
    {	
      /* Do Nothing */	
    }
  }
}

/************************************************************************************
* Service Name: Port_GetVersionInfo
* Service ID[hex]: 0x03
* Sync/Async: Synchronous
* Reentrancy: Non reentrant
* Parameters (in): None
* Parameters (inout): None
* Parameters (out): versioninfo - Pointer to where to store the version information of this module.
* Return value: None
* Description: Returns the version information of this module.
************************************************************************************/
#if (PORT_VERSION_INFO_API == STD_ON)
void Port_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
  #if (PORT_DEV_ERROR_DETECT == STD_ON)
  /* check if the input configuration pointer is not a NULL_PTR */
  if(versioninfo == NULL_PTR)
  {
    Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, Port_Get_Version_Info_SID, PORT_E_PARAM_POINTER);
  }
  else
  {	
    /* Do Nothing */	
  }
  
  /* Check if the Driver is initialized before using this function */
  if(Port_Status == PORT_NOT_INITIALIZED)
  {
    Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, PORT_GET_VERSION_INFO_SID, PORT_E_UNINIT);
  }
  else
  {	
      /* Do Nothing */	
  }
  #endif
  /* Copy the module Id */
  versioninfo->moduleID = (uint16)PORT_MODULE_ID;
  /* Copy the vendor Id */
  versioninfo->vendorID = (uint16)PORT_VENDOR_ID;
  /* Copy Software Major Version */
  versioninfo->sw_major_version = (uint8)PORT_SW_MAJOR_VERSION;
  /* Copy Software Minor Version */
  versioninfo->sw_minor_version = (uint8)PORT_SW_MINOR_VERSION;
  /* Copy Software Patch Version */
  versioninfo->sw_patch_version = (uint8)PORT_SW_PATCH_VERSION;
}
#endif

/************************************************************************************
* Service Name: Port_SetPinMode
* Service ID[hex]: 0x04
* Sync/Async: Synchronous
* Reentrancy: reentrant
* Parameters (in): Pin - Port Pin ID number, Mode - New Port Pin mode to be set on port pin
* Parameters (inout): None
* Parameters (out): None
* Return value: None
* Description: Sets the port pin mode.
************************************************************************************/
#if (PORT_SET_PIN_MODE_API == STD_ON)
void Port_SetPinMode(Port_PinType Pin, Port_PinModeType Mode)
{
	#if (PORT_DEV_ERROR_DETECT == STD_ON)
		/* Check if the Driver is initialized before using this function */
		if(Port_Status == PORT_NOT_INITIALIZED)
		{
			Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, Port_Set_Pin_Mode_SID, PORT_E_UNINIT);
		}
		else
		{	/* Do Nothing */	}

		/* check if incorrect Port Pin ID passed */
		if(Pin >= PORT_CONFIGURED_PINS)
		{
			Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, Port_Set_Pin_Mode_SID, PORT_E_PARAM_PIN);
		}
		else
		{	/* Do Nothing */	}

		/* check if the Port Pin Mode passed not valid */
		if(Mode > PORT_DIO_MODE)
		{
			Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, Port_Set_Pin_Mode_SID, PORT_E_PARAM_INVALID_MODE);
		}
		else
		{	/* Do Nothing */	}

		/* check if the API called when the mode is unchangeable */
		if(Port_ConfigPtr->Pins[Pin].Pin_modeChange == STD_OFF)
		{
			Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, Port_Set_Pin_Mode_SID, PORT_E_MODE_UNCHANGEABLE);
		}
		else
		{	/* Do Nothing */	}
	#endif

	volatile uint32 * PortGpio_Ptr = NULL_PTR; /* point to the required Port Registers base address */

	switch(Port_ConfigPtr->Pins[Pin].Port_Num )
	{
		case  0: PortGpio_Ptr = (volatile uint32 *)GPIO_PORTA_BASE_ADDRESS; /* PORTA Base Address */
		 break;
		case  1: PortGpio_Ptr = (volatile uint32 *)GPIO_PORTB_BASE_ADDRESS; /* PORTB Base Address */
		 break;
		case  2: PortGpio_Ptr = (volatile uint32 *)GPIO_PORTC_BASE_ADDRESS; /* PORTC Base Address */
		 break;
		case  3: PortGpio_Ptr = (volatile uint32 *)GPIO_PORTD_BASE_ADDRESS; /* PORTD Base Address */
		 break;
		case  4: PortGpio_Ptr = (volatile uint32 *)GPIO_PORTE_BASE_ADDRESS; /* PORTE Base Address */
		 break;
		case  5: PortGpio_Ptr = (volatile uint32 *)GPIO_PORTF_BASE_ADDRESS; /* PORTF Base Address */
		 break;
	}

	if( (Port_ConfigPtr->Pins[Pin].Port_Num == 2) && (Port_ConfigPtr->Pins[Pin].Pin_Num <= 3) ) /* PC0 to PC3 */
	{
		/* Do Nothing ...  this is the JTAG pins */
		return;
	}

	if (Mode == PORT_DIO_MODE)
	{
		/* Clear the corresponding bit in the GPIOAMSEL register to disable analog functionality on this pin */
		CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ANALOG_MODE_SEL_REG_OFFSET) , Port_ConfigPtr->Pins[Pin].Pin_Num);

		/* Disable Alternative function for this pin by clear the corresponding bit in GPIOAFSEL register */
		CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ALT_FUNC_REG_OFFSET) , Port_ConfigPtr->Pins[Pin].Pin_Num);

		/* Clear the PMCx bits for this pin */
		*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_CTL_REG_OFFSET) &= ~(0x0000000F << (Port_ConfigPtr->Pins[Pin].Pin_Num * 4));

		/* Set the corresponding bit in the GPIODEN register to enable digital functionality on this pin */
		SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIGITAL_ENABLE_REG_OFFSET) , Port_ConfigPtr->Pins[Pin].Pin_Num);
	}
	else if (Mode == PORT_ALTERNATE_FUNCTION_ADC_MODE)
	{
		/* Clear the corresponding bit in the GPIODEN register to disable digital functionality on this pin */
		CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIGITAL_ENABLE_REG_OFFSET) , Port_ConfigPtr->Pins[Pin].Pin_Num);

		/* Disable Alternative function for this pin by clear the corresponding bit in GPIOAFSEL register */
		CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ALT_FUNC_REG_OFFSET) , Port_ConfigPtr->Pins[Pin].Pin_Num);

		/* Clear the PMCx bits for this pin */
		*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_CTL_REG_OFFSET) &= ~(0x0000000F << (Port_ConfigPtr->Pins[Pin].Pin_Num * 4));

		/* Set the corresponding bit in the GPIOAMSEL register to enable analog functionality on this pin */
		SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ANALOG_MODE_SEL_REG_OFFSET) , Port_ConfigPtr->Pins[Pin].Pin_Num);
	}
	else /* Another mode */
	{
		/* Clear the corresponding bit in the GPIOAMSEL register to disable analog functionality on this pin */
		CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ANALOG_MODE_SEL_REG_OFFSET) , Port_ConfigPtr->Pins[Pin].Pin_Num);

		/* Enable Alternative function for this pin by clear the corresponding bit in GPIOAFSEL register */
		SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ALT_FUNC_REG_OFFSET) , Port_ConfigPtr->Pins[Pin].Pin_Num);

		/* Set the PMCx bits for this pin */
		*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_CTL_REG_OFFSET) |= (Mode & 0x0000000F << (Port_ConfigPtr->Pins[Pin].Pin_Num * 4));

		/* Set the corresponding bit in the GPIODEN register to enable digital functionality on this pin */
		SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIGITAL_ENABLE_REG_OFFSET) , Port_ConfigPtr->Pins[Pin].Pin_Num);
	}
}
#endif

