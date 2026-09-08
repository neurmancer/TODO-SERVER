### To-do list

> yeah a repo where I am trying to make a to-do app...I have a to-do readme...

- [ ] use goto for cleanup pattern in database.c

- [ ] better abstraction maybe...dunno yet...

- [ ] css and js support (well route table can handle it with a few tweaks)

- [ ] markdown style rendering with a lightweight JS framework(yeah I am not gonna do that myself bruh nope! Enough finite automata for me)

- [X] Man...it's getting late



```c
//Well I don't have the mental capacity to implement this rn but I can't forget so here is a dynamic_http struct draft

int http_send_response(
    int client_sock,
    int status,
    const char *content_type,
    const char *body
);

//Yeah...in a notes markdown I am still writing fucking C
//And function returns would somethingl like that:
return(
    http_send_response(
    client_sock,
    200,
    "text/html",
    page)
);

```