using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Threading;

namespace FileSystemSearch
{
	class SearchResultsView : HwndHost
	{
		private IntPtr childView;
		private ManualResetEvent createdEvent = new ManualResetEvent(false);

		protected override HandleRef BuildWindowCore(HandleRef hwndParent)
		{
			childView = CreateView(hwndParent.Handle, (int)Width, (int)Height);
			var childHwnd = GetHwnd(childView);

			Dispatcher.BeginInvoke(DispatcherPriority.Normal, (Action)(() =>
			{
				InitializeView(childView);

				SizeChanged += OnSizeChanged;
				ResizeView(childView, (int)ActualWidth, (int)ActualHeight);
				createdEvent.Set();
			}));

			return new HandleRef(this, childHwnd);
		}

		protected override void DestroyWindowCore(HandleRef hwnd)
		{
			Cleanup();
		}

		private void OnSizeChanged(object sender, SizeChangedEventArgs e)
		{
			if (childView != IntPtr.Zero)
				ResizeView(childView, (int)e.NewSize.Width, (int)e.NewSize.Height);
		}

		public void AddItem(IntPtr findData, IntPtr itemPath)
		{
			createdEvent.WaitOne();

			Dispatcher.Invoke(() =>
			{
				if (childView != IntPtr.Zero)
					AddItemToView(childView, findData, itemPath);
			});
		}

		public void Cleanup()
		{
			if (childView != IntPtr.Zero)
			{
				SizeChanged -= OnSizeChanged;
				DestroyView(childView);
				childView = IntPtr.Zero;
			}
		}

		[DllImport("SearchResultsView.dll")]
		private static extern IntPtr CreateView(IntPtr parent, int width, int height);

		[DllImport("SearchResultsView.dll")]
		private static extern void InitializeView(IntPtr view);

		[DllImport("SearchResultsView.dll")]
		private static extern void ResizeView(IntPtr view, int width, int height);

		[DllImport("SearchResultsView.dll")]
		private static extern IntPtr GetHwnd(IntPtr view);

		[DllImport("SearchResultsView.dll")]
		private static extern void DestroyView(IntPtr view);

		[DllImport("SearchResultsView.dll")]
		private static extern void AddItemToView(IntPtr view, IntPtr findData, IntPtr itemPath);
	}
}
