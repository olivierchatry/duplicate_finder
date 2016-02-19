# building

On OSX / linux, as long as you have boost installed ( recent version ), just run `sh build/nix/build.sh`. It will build the code and you will end up with an `a.out` you can execute.

On windows, I used vs2015 community. boost needs to be setup within visual for it to build. Just use the usual build command in visual. The solution can be found in `build/vs2015` the result will be in the same folder, and follows the usual visual hierarchy.

The programs support multiple paths as arguments as well as these options :
* `+data` duplicate find by data match
* `+name` duplicate find by name match
* `-data` duplicate find without data match
* `-name` duplicate find without name match

So you can do :
`duplicate_finder +data test\ -data +name test2\`

Which will match only data for folder test, and only name for folder test2. The result will be merger between paths, so you can compare two different folder.

Default is `+name -data`.

# explanation
The program works in three phase :

## first phase, discovering duplicate files

Using boost file system, it will scan the give folder for all files. This is done using *recursive_directory_iterator* which will recuse in all sub-folders.

* For each *fs::path* :
	* compute the CRC32 of the file using some bytes or the file name.
	* match the CRC32 against a multimap.
	* if we have CRC32 match ( one or more ), it will then compare the filename and/or data. ( We can have collision in the hash, hence the multimap ).
	* if we have a match, it will happen the *fs::path* to a list of duplicate, which is a map, indexed by the first *fs::path* found.
  * if we do not have a match, the CRC32 and the *fs::path* is added to the multimap.


> Function : *find_duplicate*, *file_compare*, *file_crc*, *find_duplicate*

## second phase, reducing result to merge same folders

After the first phase, we have a list of files that are duplicated, we know need to merge them when they belongs to the same folder ( as well as indicating which files are equal when the filename is not the same ).

> Note : an entry of a duplicate contains a list of *fs::path* that are equals

To do that :

* For each entry in duplicate
	* build a string from the *parent_path* of all *fs::path* in the entry.
	* build a set of the filename from the *fs::path*, so that we can display duplicate with different filename.
	* insert inside a map, using the *parent_path* concatenated string as the key, the concatenation of the set of filename.

> Function : *reduce_result*

## third phase, display

After we reduced our result, display is very easy, we just go through the reduced map, display the key ( which is the concatenation of all paths ), and then display the set of files that matches.

# Thought about the code

After consideration, I think the code is cleaner / leaner like this. I like C++ as much as I like C, and since there is no complex interaction between data, or no generecity that can be sanely applied, I think it was the best choice.

Code wise, I tried to be as clean as possible, without thinking to much about memory optimization. It is certainly possible to store some of the data into a temp database, on disk, so that it will not clutter the memory.

Memory which is one of the point where the program can crash : every temp steps are stored in memory, which means that if we scan a big hierarchy, it can die out of memory.

I did not consider threading, my experience with file system and hard-drive told me that we will be mainly be io bound, so threading will actually create more contention.

The search can be slow, depending on the amount of files, their size, and the options chosen.

I added a "test" folder that contains the sample case of the subject, with a file in "a" that have filename that is different "X - Copy.txt" but contains the same data as "x". I also added a file in folder C that has the same name "x.txt" but has different data inside.

Command line parsing is not really top notch ( I did not want to use boost program option, as it looked like a bazooka to kill a fly).
