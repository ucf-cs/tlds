---
layout: tervel_documentation
---

# Installing TLDS

## System Requirements

Supported platforms:

*   Ubuntu Linux


## Downloading TLDS

Clone the TLDS from the git repo:

{% highlight bash %}
$ git clone {{ site.gitrepo }}
{% endhighlight %}

## Building TLDS

### Dependencies

* g++4.8.4 or above

* pthreads

* boost 1.37 or above

* Intel TBB

* tcmalloc (packaged in libgoogle-perftools-dev on Ubuntu)

* GSL


### Building

{% highlight bash %}
git clone https://github.com/ucf-cs/tlds
mv tlds trans-dev
cd trans-dev
./bootstrap.sh
cd ../trans-compile
../trans-dev/configure
make -j8    #-j followed by a number runs make with that many threads
./src/trans  #this runs the tester without any options
{% endhighlight %}

## Next Steps

Now you can add TLDS containers and algorithms into your own applications.
See [user manual](tlds-user-manual.html) for more information.
Let us know if you have any questions!
