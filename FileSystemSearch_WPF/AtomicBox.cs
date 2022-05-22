using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace FileSystemSearch
{
	public struct AtomicBox<T>
	{
		private volatile object boxedItem;

		public void Set(ref T item)
		{
			boxedItem = item;
		}

		public T Get()
		{
			return (T)boxedItem;
		}
	}
}
