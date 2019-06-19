# Управление ккт через Websocket

### данные передаются в формате:
```ts
{
    "event": "event_name",  // Событие
    "data": any             // Данные 
}
```
## Ответ:
```ts
{
    "event": "onevent_name",   // событие
    "data": {
        "error": boolean,       // Ошибка (да/нет)
        "error_code": number    // Код ошибки
        "text": string,         // Текст ошибки
    }
}
```

## Примеры:

### [Открыть смену]:
`Запрос:`
```js
{
    event: "open_shift", 
    data: {
        cashier: "Сидр Петров" // Кассир
    }
}
```
`Ответ`
```js
    {
        event: "onopen_shift",
        data: {
            error: false
        }
    }
```

### [Завершение смены]:
`Запрос`
```js
{
    event: "close_shift", 
    data: {
        cashier: "Сидр Петров" // Кассир
    }
}
```
`Ответ`
```js
    {
        event: "onclose_shift",
        data: {
            error: false
        }
    }
```

### [Принудительное(Аварийное) завершение смены]:
`Запрос`
```js
{
    event: "force_close_shift", 
}
```
`Ответ`
```js
{
    event: "onforce_close_shift",
    data: {
        error: false
    }
}
```

### [Новый документ (чек)]:
`Запрос`
```js
{
    event: "create_doc", 
    data: {
        cashier: "Сидр Петров", // Кассир
        doc_type: 2,            // Режим и тип документа (2-продажа, 3-возврат),
        wares: [
            {
                name: "Водка Айс 0,5л 40%", // Наименование товара
                barcode: "123123",          // ШК товара
                tax_number: 0,              // НДС 0 - 20%, 1 - 10%,
                price: 123.45,              // Цена за 1 ед. измерения,
                quantity: 2,                // Количество
            }
        ]
    }
}
```
`Ответ`
```js
    {
        event: "oncreate_doc",
        data: {
            error: false,           
            amount: 2469,               // Сумма для оплаты (в копейках)
            cashier: "Сидр Петров",     // Касир
            error_code: 0,              // Код ошибки
            text: "Успешно"             // Сообщение
        }
    }
```

### [Оплата]:
`Запрос`
```js
{
    event: "pay", 
    data: {
        amount: 2469 // Количество (в копейкай)
    }
}
```
`Ответ`
```js
    {
        event: "onpay",
        data: {
            error: false,     
            rrn: "LINK_916913195426",   // ссылка платежа
            auth_id: 1,                 // id оплаты
            error_code: 0,              // Код ошибки
            text: "Успешно"             // Сообщение
        }
    }
```

### [Отмена платежа]:
`Запрос`
```js
{
    event: "cancel_payment", 
    data: {
        auth_id: 1,                 // id оплаты
    }
}
```
`Ответ`
```js
    {
        event: "oncancel_payment",
        data: {
            error: false,
            response_code: 0,
            text: "Успешно",
        }
    }
```

### [Отмена платежа по ссылке]:
`Запрос`
```js
{
    event: "cancel_payment_by_link", 
    data: {
        rrn: "LINK_916913195426",   // Ссылка платежа,
        amount: 2469                // сумма платежа (в копейкай)
    }
}
```
`Ответ`
```js
    {
        event: "oncancel_payment_by_link",
        data: {
            error: false,
            response_code: 0,
            text: "Успешно",
        }
    }
```
