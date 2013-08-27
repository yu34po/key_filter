#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>
#include <ngx_config.h>
//page info
char* page =  "<html>\r\n<head><title>there is a word not allowed for this page</title></head>\r\n<body bgcolor=\"white\">\r\n<center><h1>there is a word not allowed for this page</h1></center>\r\n";
typedef struct{
    ngx_str_t loc;
    ngx_array_t* key_array;
}ngx_http_key_loc_conf_t;
static char* ngx_http_key_filter(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
static ngx_int_t ngx_http_key_filter_init(ngx_conf_t* cf);
static ngx_int_t ngx_http_key_body_filter(ngx_http_request_t* r, ngx_chain_t* in);
static ngx_int_t ngx_http_key_header_filter(ngx_http_request_t* r);

static void* ngx_http_key_create_conf(ngx_conf_t* cf);
static char* ngx_http_key_merge_conf(ngx_conf_t* cf, void* parent, void* child);
static ngx_int_t ngx_http_key_match_process(ngx_http_request_t* r, ngx_buf_t* b);
static ngx_command_t ngx_http_key_filter_commands[] = {
    {
    ngx_string("loc"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
    ngx_http_key_filter,  
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_key_loc_conf_t, loc),
    NULL},
    ngx_null_command
};
static ngx_http_module_t ngx_http_key_filter_module_ctx = {
    NULL,
    ngx_http_key_filter_init,

    NULL,
    NULL,
    NULL,
    NULL,
    
    ngx_http_key_create_conf,
    ngx_http_key_merge_conf
};

ngx_module_t ngx_http_key_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_key_filter_module_ctx,
    ngx_http_key_filter_commands,
    NGX_HTTP_MODULE,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NGX_MODULE_V1_PADDING
};

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt ngx_http_next_body_filter;
static ngx_int_t ngx_http_key_filter_init(ngx_conf_t* cf){
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_key_header_filter;
    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_key_body_filter;
    return NGX_OK;
}

static ngx_int_t ngx_http_key_header_filter(ngx_http_request_t* r){
    r->filter_need_in_memory = 1;
    return ngx_http_next_header_filter(r);
}
static char* ngx_http_key_filter(ngx_conf_t* cf, ngx_command_t* cmd, void* conf){
    ngx_http_key_loc_conf_t* slcf = (ngx_http_key_loc_conf_t*)conf;
    ngx_conf_set_str_slot(cf, cmd, conf);
    if(conf == NULL){
        return NGX_CONF_ERROR;
    }
    char loc[128];
    char buffer[256];
    char temp[48];
    ngx_memzero(loc, sizeof(loc));
    ngx_memzero(buffer, sizeof(buffer));
    ngx_memzero(temp, sizeof(temp));
    ngx_str_t* array_temp;
    ngx_memcpy(loc, (char*)(slcf->loc.data), slcf->loc.len);
    ngx_int_t fd = open(loc,O_RDONLY);
    if( fd < 0){
        return NGX_CONF_ERROR;
    }
    ngx_int_t size = read(fd, buffer, sizeof(buffer));
    if(size <= 0){
        return NGX_CONF_ERROR;
    }
    close(fd);
    if(NULL == slcf->key_array){
        slcf->key_array = ngx_array_create(cf->pool, 4, sizeof(ngx_str_t));
        if(NULL == slcf->key_array){
            return NGX_CONF_ERROR;
        }
    }
    ngx_int_t i = 0, j = 0, begin = 0, end = 0,find = 0;
    for(; i <= size; i++){
        if(buffer[i] == ' ' || buffer[i] == '\n'){
            end = i;
            if(find)
                continue;
        }else{
            if(find){
                begin = i;
                find = 0;
            }
            continue;
        }
        find = 1;
        for(j = 0; j < end - begin; j++){
            temp[j] = buffer[begin + j];
        }
        temp[end - begin] = '\0';
        if(0 == strlen(temp)){
            continue;
        }
        array_temp = ngx_array_push(slcf->key_array);
        if(NULL == array_temp){
            return NGX_CONF_ERROR;
        }
        array_temp->data = ngx_palloc(cf->pool, strlen(temp) + 1);
        if(NULL == array_temp->data){
            return NGX_CONF_ERROR;
        }
        ngx_memcpy(array_temp->data, (u_char*)temp, strlen(temp) + 1);
        array_temp->len = strlen(temp);
        memset(temp, 0x00, sizeof(temp));
        array_temp = NULL;
    }
//    ngx_uint_t k = 0;
//    ngx_str_t* q = (ngx_str_t*) slcf->key_array->elts;
//    for(; k < slcf->key_array->nelts; k++){
//        printf("%d\n", q[k].len);
//        printf("%s\n", q[k].data);
//    }

    return NGX_CONF_OK;
}
static ngx_int_t ngx_http_key_body_filter(ngx_http_request_t* r, ngx_chain_t* in){
    ngx_int_t rc;
    ngx_chain_t* cl, *temp; 
    ngx_http_key_loc_conf_t* slcf;
    ngx_log_t* log;
    log = r->connection->log;
    slcf = ngx_http_get_module_loc_conf(r, ngx_http_key_filter_module);
    if(slcf == NULL){
        return ngx_http_next_body_filter(r, in);
    }
    if(in == NULL){
        return ngx_http_next_body_filter(r, in);
    }
    temp = in;
    for(cl = temp; cl; cl = cl->next){
        rc = ngx_http_key_match_process(r, cl->buf);
        if(rc == NGX_DECLINED){
            continue;
        }else if(rc == NGX_ERROR) {
            goto failed;
        }
        for(cl = in; cl; cl = cl->next){
            ngx_free_chain(r->pool, cl);
        }
        ngx_buf_t* b = ngx_create_temp_buf(r->pool, 200);
        b->start = b->pos = (u_char*)page;
        b->last = b->pos + strlen(page);
        b->last_buf = 1;
        b->memory = 1;
        cl = ngx_alloc_chain_link(r->pool);
        cl->buf = b;
        cl->next = NULL;
        return ngx_http_next_body_filter(r, cl);
    }
    return ngx_http_next_body_filter(r, in);
failed:
    ngx_log_error(NGX_LOG_ERR, log, 0,"ngx_http_key_body_filter error." );
    return NGX_ERROR;
}
static void get_next(const char target[], ngx_int_t next[]){
    ngx_int_t i , j , len;
    next[0] = -1;
    for(j = 1; target[j] != '\0'; j++){
        for(len = j - 1; len >= 1; len--){
            for(i = 0; i < len; i++)
                if(target[i] != target[j - len + i])
                    break;
                if(i == len){
                    next[j] = len;
                    break;
                }
        }
        if(len < 1)
            next[j] = 0;
        
    }
}
ngx_int_t kmp_search(const char pattern[], const char target[]){
    ngx_int_t i = 0, j = 0;
    ngx_int_t next[128];
    get_next(target, next);
    while(pattern[i] != '\0' && target[j] != '\0'){
        if(pattern[i] == target[j]){
            i++;
            j++;
        }else{
            j = next[j];
            if(-1 == j){
                i++;
                j++;
            }
        }
    }
    if(target[j] == '\0')
        return (i - strlen(target) + 1);
    else 
        return 0;
}
static ngx_int_t ngx_http_key_match_process(ngx_http_request_t* r, ngx_buf_t* b){
       ngx_http_key_loc_conf_t* conf = ngx_http_get_module_loc_conf(r, ngx_http_key_filter_module);
      // char* pos, *temp;
       const char* l = (const char*)b->pos;
       size_t len = b->last - b->pos;
       ngx_str_t* q = (ngx_str_t*) conf->key_array->elts;
       ngx_uint_t n = 0;
       for(; n < conf->key_array->nelts; n++){
            const char* k = (const char*) q[n].data;
            size_t k_len = q[n].len;
            if(0 == len || 0 == k_len){
                return NGX_ERROR;
            }
            if(len < k_len){
                return NGX_DECLINED;
            }
            if(kmp_search(l, k))
                return NGX_OK;
      /*      temp = (char*) l + len - k_len;
            for(pos = (char*)l; pos <= temp; pos++){
                if(pos[0] == k[0] && memcmp(pos, k, k_len) == 0)
                    return NGX_OK;
            }*/
       }
       return NGX_DECLINED;
}
static void* ngx_http_key_create_conf(ngx_conf_t* cf){
    printf("create_key_conf\n");
    ngx_http_key_loc_conf_t* conf;
    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_key_loc_conf_t));
    if(conf == NULL){
        return NGX_CONF_ERROR;
    }
    conf->loc.len = 0;
    conf->loc.data = NULL;
    conf->key_array = NULL;
    return conf;
}
static char* ngx_http_key_merge_conf(ngx_conf_t* cf, void* parent, void* child){
   printf("merge_key_conf\n");
    ngx_http_key_loc_conf_t* prev = parent;
    ngx_http_key_loc_conf_t* conf = child;
    if(conf->key_array == NULL){
       if(prev->key_array == NULL){
            conf->key_array = ngx_array_create(cf->pool, 4, sizeof(ngx_str_t));
            if(conf->key_array == NULL){
                return NGX_CONF_ERROR;
            }
       }else{
            conf->key_array = prev->key_array;
       }
    }
    ngx_conf_merge_str_value(conf->loc, prev->loc, "");
    return NGX_CONF_OK;
}
