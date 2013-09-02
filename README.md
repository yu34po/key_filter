key_filter
==========

nginx key filter module
简单的基于关键字网页过滤模块，在body filter中加入key_filter模块，对类型为text的response内容进行关键字匹配，匹配采用KMP算法，关键字从
“key”指令参数对应的的文件路径中提取文件，关键字以空格或换行分隔，若匹配成功直接返回简单的错误页面。
后期需要改进的是对KMP算法中对关键字的next[]数组预先构造哈希表，支持关键字的热切换操作。
